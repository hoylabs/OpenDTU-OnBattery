// SPDX-License-Identifier: GPL-2.0-or-later
#include <Utils.h>
#include <powermeter/json/http/Provider.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <mbedtls/sha256.h>
#include <base64.h>
#include <ESPmDNS.h>
#include <LogHelper.h>
#include "Statistic.h"

#undef TAG
static const char* TAG = "powerMeter";
static const char* SUBTAG = "HTTP/JSON";

namespace PowerMeters::Json::Http {

Provider::~Provider()
{
    _taskDone = false;

    std::unique_lock<std::mutex> lock(_pollingMutex);
    _stopPolling = true;
    lock.unlock();

    _cv.notify_all();

    if (_taskHandle != nullptr) {
        while (!_taskDone) { delay(10); }
        _taskHandle = nullptr;
    }
}

bool Provider::init()
{
    for (uint8_t i = 0; i < POWERMETER_HTTP_JSON_MAX_VALUES; i++) {
        auto const& valueConfig = _cfg.Values[i];

        _httpGetters[i] = nullptr;

        if (i == 0 || (_cfg.IndividualRequests && valueConfig.Enabled)) {
            _httpGetters[i] = std::make_unique<HttpGetter>(valueConfig.HttpRequest);
        }

        if (!_httpGetters[i]) { continue; }

        if (_httpGetters[i]->init()) {
            _httpGetters[i]->addHeader("Content-Type", "application/json");
            _httpGetters[i]->addHeader("Accept", "application/json");
            continue;
        }

        DTU_LOGE("Initializing HTTP getter for value %d failed: %s",
                 i + 1, _httpGetters[i]->getErrorText());
        return false;
    }

    return true;
}

void Provider::loop()
{
    if (_taskHandle != nullptr) { return; }

    std::unique_lock<std::mutex> lock(_pollingMutex);
    _stopPolling = false;
    lock.unlock();

    uint32_t constexpr stackSize = 6144;
    xTaskCreate(Provider::pollingLoopHelper, "PM:HTTP+JSON",
            stackSize, this, 1/*prio*/, &_taskHandle);
}

void Provider::pollingLoopHelper(void* context)
{
    auto pInstance = static_cast<Provider*>(context);
    pInstance->pollingLoop();
    pInstance->_taskDone = true;
    vTaskDelete(nullptr);
}

void Provider::pollingLoop()
{
    std::unique_lock<std::mutex> lock(_pollingMutex);

    // debug variables
    uint32_t lastPrint = 0;
    uint16_t dataCounter = 0;
    uint16_t identicalDataCounter = 0;
    uint16_t errorCount = 0;
    float lastTotalPower = 0.0f;
    WeightedAVG<uint32_t> avgPollTime{50};     // average poll time
    WeightedAVG<uint32_t> avgIntervalTime{50}; // average interval time

    while (!_stopPolling) {
        uint32_t elapsedMillis = millis() - _lastPoll;
        uint32_t intervalMillis = _cfg.PollingIntervalMs;
        if (_lastPoll > 0 && elapsedMillis < intervalMillis) {
            auto sleepMs = intervalMillis - elapsedMillis;
            sleepMs = std::max(sleepMs, intervalMillis / 2); // to avoid too fast polling
            _cv.wait_for(lock, std::chrono::milliseconds(sleepMs),
                    [this] { return _stopPolling; }); // releases the mutex
            continue;
        }

        // record the average interval time
        uint32_t pollStart = millis();
        if (_lastPoll > 0) { avgIntervalTime.addNumber(pollStart - _lastPoll); }

        _lastPoll = pollStart; // used for calculating the next polling interval

        lock.unlock(); // polling can take quite some time
        auto res = poll();
        lock.lock();

        uint32_t pollEnd = millis();

        if (std::holds_alternative<String>(res)) {
            DTU_LOGE("%s", std::get<String>(res).c_str());
            errorCount++;
        } else {
            float currentTotalPower = getPowerTotal();
            if ((currentTotalPower == lastTotalPower) && (currentTotalPower != 0.0f)) { identicalDataCounter++; }
            lastTotalPower = currentTotalPower;

            DTU_LOGD("New total: %.2fW", currentTotalPower);
        }

        // record the average poll time
        avgPollTime.addNumber(pollEnd - pollStart);

        // prevent overflow of the counters by halving them when reaching 10.000,
        // which is sufficient for calculating the percentage of data
        if (dataCounter == 10000) {
            dataCounter /= 2;
            identicalDataCounter /= 2;
            errorCount /= 2;
        }
        dataCounter++;

        // periodic information output
        if (pollEnd - lastPrint > 30 * 1000) {
            lastPrint = pollEnd;
            DTU_LOGI("Average interval time: %ums, [Min: %u, Max: %u]",
                avgIntervalTime.getAverage(), avgIntervalTime.getMin(), avgIntervalTime.getMax());
            DTU_LOGI("Average poll time: %ums, [Min: %u, Max: %u]",
                avgPollTime.getAverage(), avgPollTime.getMin(), avgPollTime.getMax());

            if (dataCounter >= 10) {
                DTU_LOGI("Http/Poll errors: %.1f%% [%u of %u polls]",
                    (static_cast<float>(errorCount) / static_cast<float>(dataCounter)) * 100.0f,
                    errorCount, dataCounter);
                DTU_LOGI("Identical data: %.1f%% [%u of %u polls]",
                    (static_cast<float>(identicalDataCounter) / static_cast<float>(dataCounter)) * 100.0f,
                    identicalDataCounter, dataCounter);
            }
        }
    }
}

Provider::poll_result_t Provider::poll()
{
    JsonDocument jsonResponse;

    auto prefixedError = [](uint8_t idx, char const* err) -> String {
        String res("Value ");
        res.reserve(strlen(err) + 16);
        return res + String(idx + 1) + ": " + err;
    };

    for (uint8_t i = 0; i < POWERMETER_HTTP_JSON_MAX_VALUES; i++) {
        auto const& cfg = _cfg.Values[i];

        if (!cfg.Enabled) {
            continue;
        }

        auto const& upGetter = _httpGetters[i];

        if (upGetter) {
            auto res = upGetter->performGetRequest();
            if (!res) {
                return prefixedError(i, upGetter->getErrorText());
            }

            auto pStream = res.getStream();
            if (!pStream) {
                return prefixedError(i, "Programmer error: HTTP request yields no stream");
            }

            const DeserializationError error = deserializeJson(jsonResponse, *pStream);
            if (error) {
                String msg("Unable to parse server response as JSON: ");
                return prefixedError(i, String(msg + error.c_str()).c_str());
            }
        }

        auto pathResolutionResult = Utils::getJsonValueByPath<float>(jsonResponse, cfg.JsonPath);
        if (!pathResolutionResult.second.isEmpty()) {
            return prefixedError(i, pathResolutionResult.second.c_str());
        }

        // this value is supposed to be in Watts and positive if energy is consumed
        float newValue = pathResolutionResult.first;

        switch (cfg.PowerUnit) {
            case Unit_t::MilliWatts:
                newValue /= 1000;
                break;
            case Unit_t::KiloWatts:
                newValue *= 1000;
                break;
            default:
                break;
        }

        if (cfg.SignInverted) { newValue *= -1; }

        {
            auto scopedLock = _dataCurrent.lock();
            switch (i) {
                case 0:
                    _dataCurrent.add<DataPointLabel::PowerL1>(newValue);
                    break;

                case 1:
                    _dataCurrent.add<DataPointLabel::PowerL2>(newValue);
                    break;

                case 2:
                    _dataCurrent.add<DataPointLabel::PowerL3>(newValue);
                    break;

                default:
                    break;
            }
        }
    }

    return _dataCurrent;
}

bool Provider::isDataValid() const
{
    uint32_t age = millis() - getLastUpdate();

    // consider data valid if last update was within 3 polling intervals, but at least 5 seconds
    return getLastUpdate() > 0 && (age < std::max(5000u, 3 * _cfg.PollingIntervalMs));
}

} // namespace PowerMeters::Json::Http
