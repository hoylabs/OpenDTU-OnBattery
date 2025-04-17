// SPDX-License-Identifier: GPL-2.0-or-later

#include <gridcharger/trucki/Provider.h>
#include <gridcharger/trucki/DataPoints.h>
#include <battery/Controller.h>
#include <powermeter/Controller.h>
#include <PowerLimiter.h>
#include <Utils.h>
#include <LogHelper.h>

#undef TAG
static const char* TAG = "gridCharger";
static const char* SUBTAG = "Trucki";

namespace GridChargers::Trucki {

bool Provider::init()
{
    DTU_LOGI("Initialize Trucki AC charger interface...");

    auto const& config = Configuration.get();
    auto const& ipAddress = IPAddress(config.GridCharger.Trucki.IpAddress);

    if (ipAddress.toString() == "0.0.0.0") {
        DTU_LOGE("Invalid IP address: %s", ipAddress.toString().c_str());
        return false;
    }

    HttpRequestConfig httpRequestConfig;
    strlcpy(httpRequestConfig.Url, ("http://" + ipAddress.toString() + "/jsonlive").c_str(), sizeof(httpRequestConfig.Url));
    httpRequestConfig.Timeout = HTTP_REQUEST_TIMEOUT_MS;

    DTU_LOGE("Request URL: %s", httpRequestConfig.Url);

    _httpGetter = std::make_unique<HttpGetter>(httpRequestConfig);

    if (!_httpGetter->init()) {
        DTU_LOGE("Initializing HTTP getter failed: %s", _httpGetter->getErrorText());
        return false;
    }

    return true;
}

void Provider::deinit()
{
    _pollingTaskDone = false;

    std::unique_lock<std::mutex> pollingLock(_pollingMutex);
    _stopPolling = true;
    pollingLock.unlock();

    _cv.notify_all();

    if (_pollingTaskHandle != nullptr) {
        while (!_pollingTaskDone) { delay(10); }
        _pollingTaskHandle = nullptr;
    }
}

void Provider::loop()
{
    if (_pollingTaskHandle == nullptr) {
        std::unique_lock<std::mutex> lock(_pollingMutex);
        _stopPolling = false;
        lock.unlock();

        uint32_t constexpr stackSize = 4096;
        xTaskCreate(Provider::pollingLoopHelper, "TruckiPolling",
                stackSize, this, 1/*prio*/, &_pollingTaskHandle);
    }
}

void Provider::pollingLoopHelper(void* context)
{
    auto pInstance = static_cast<Provider*>(context);
    pInstance->pollingLoop();
    pInstance->_pollingTaskDone = true;
    vTaskDelete(nullptr);
}

void Provider::pollingLoop()
{
    std::unique_lock<std::mutex> lock(_pollingMutex);

    while (!_stopPolling) {
        auto elapsedMillis = millis() - _lastPoll;

        if (_lastPoll > 0 && elapsedMillis < POLLING_INTERVAL_MS) {
            auto sleepMs = POLLING_INTERVAL_MS - elapsedMillis;
            _cv.wait_for(lock, std::chrono::milliseconds(sleepMs),
                    [this] { return _stopPolling; }); // releases the mutex
            continue;
        }

        _lastPoll = millis();

        lock.unlock(); // polling can take quite some time
        poll();
        lock.lock();
    }
}

void Provider::poll()
{
    if (!_httpGetter) {
        DTU_LOGE("HTTP getter not initialized");
        return;
    }

    auto result = _httpGetter->performGetRequest();

    if (!result) {
        DTU_LOGE("Failed to get data from Trucki: %s", _httpGetter->getErrorText());
        return;
    }

    auto pStream = result.getStream();
    if (!pStream) {
        DTU_LOGE("Programmer error: HTTP request yields no stream");
        return;
    }

    JsonDocument data;
    const DeserializationError error = deserializeJson(data, *pStream);
    if (error) {
        DTU_LOGE("Unable to parse server response as JSON: %s", error.c_str());
        return;
    }

    using Label = GridChargers::Trucki::DataPointLabel;

    {
        auto scopedLock = _dataCurrent.lock();

        addStringToDataPoints<Label::ZEPC>(data, "ZEPCPOWER");
        addStringToDataPoints<Label::State>(data, "MWPCSTATE");
        addFloatToDataPoints<Label::Temperature>(data, "TEMP");
        addFloatToDataPoints<Label::Efficiency>(data, "MWEFFICIENCY");
        addFloatToDataPoints<Label::DayEnergy>(data, "DAYENERGY");
        addFloatToDataPoints<Label::TotalEnergy>(data, "TOTALENERGY");

        addFloatToDataPoints<Label::AcVoltage>(data, "VGRID");
        addFloatToDataPoints<Label::AcPowerSetpoint>(data, "SETACPOWER");
        addFloatToDataPoints<Label::AcPower>(data, "MQTT_ACDISPLAY_VALUE");

        addFloatToDataPoints<Label::DcVoltage>(data, "VBAT");
        addFloatToDataPoints<Label::DcVoltageSetpoint>(data, "VOUTSET");
        addFloatToDataPoints<Label::DcPower>(data, "DCPOWER");
        addFloatToDataPoints<Label::DcCurrent>(data, "IOUT");

        addFloatToDataPoints<Label::DcVoltageOffline>(data, "VOUTOFFLINE");
        addFloatToDataPoints<Label::DcCurrentOffline>(data, "IOUTOFFLINE");
        addFloatToDataPoints<Label::MinAcPower>(data, "MQTT_MINPOWER_VALUE");
        addFloatToDataPoints<Label::MaxAcPower>(data, "POWERLIMIT");
    }

    _stats->updateFrom(_dataCurrent);
}
} // namespace GridChargers::Trucki
