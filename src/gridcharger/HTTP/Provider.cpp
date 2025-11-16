// SPDX-License-Identifier: GPL-2.0-or-later

#include <gridcharger/HTTP/Provider.h>
#include <gridcharger/HTTP/DataPoints.h>
#include <battery/Controller.h>
#include <powermeter/Controller.h>
#include <PowerLimiter.h>
#include <Utils.h>
#include <WiFiUdp.h>
#include <LogHelper.h>

#undef TAG
static const char* TAG = "gridCharger";
static const char* SUBTAG = "HTTP";

namespace GridChargers::HTTP {

bool Provider::init()
{
    DTU_LOGI("Initialize HTTP AC charger interface...");

    auto const& config = Configuration.get();
    String  url = config.GridCharger.HTTP.url;
    uri_on = url + config.GridCharger.HTTP.uri_on;
    uri_off = url + config.GridCharger.HTTP.uri_off;
    uri_stats = url + config.GridCharger.HTTP.uri_stats;
    uri_powerparam = config.GridCharger.HTTP.uri_powerparam;
    maximumAcPower = config.GridCharger.HTTP.AcPower;
    return true;
}

void Provider::deinit()
{
    DTU_LOGI("DEInitialize HTTP AC charger interface...");
    PowerOFF();
    _dataPollingTaskDone = false;

    std::unique_lock pollingLock(_dataPollingMutex);
    _stopPollingData = true;
    pollingLock.unlock();

    _dataPollingCv.notify_all();

    if (_dataPollingTaskHandle != nullptr) {
        while (!_dataPollingTaskDone) { delay(10); }
        _dataPollingTaskHandle = nullptr;
    }

    _httpGetter = nullptr;
    _httpRequestConfig = nullptr;
}

void Provider::loop()
{
    powerControlLoop();

    // Ensure polling task is started only once
    if (_dataPollingTaskHandle == nullptr) {
        std::unique_lock lock(_dataPollingMutex);
        if (!_stopPollingData) {
            uint32_t constexpr stackSize = 6144;
            xTaskCreate(dataPollingLoopHelper, "HTTPPolling",
                        stackSize, this, 1/*prio*/, &_dataPollingTaskHandle);
        }
        // No need to unlock here, lock will be released automatically
    }
}

void Provider::PowerON()
{
    if (!send_http(uri_on))
    {
        return;
    }
    powerstate=true;
    DTU_LOGI("Power ON\r\n");
}

void Provider::PowerOFF()
{
    if (!send_http(uri_off))
    {
        return;
    };
    powerstate=false;
    DTU_LOGI("Power OFF\r\n");
}

bool Provider::send_http(String Url)
{
    HttpRequestConfig HttpRequest;
    strlcpy(HttpRequest.Url, Url.c_str(), sizeof(HttpRequest.Url));
    HttpRequest.Timeout = 60; // 60 seconds
    _httpGetter = std::make_unique<HttpGetter>(HttpRequest);
    DTU_LOGI("Start sending to URL: %s\r\n",Url.c_str());

    if (!_httpGetter->init()) {
        DTU_LOGE("ERROR INIT HttpGetter %s\r\n", _httpGetter->getErrorText());
        _httpGetter = nullptr;  // Ensure cleanup on error
        return false;
    }

    if (!_httpGetter->performGetRequest()) {
        DTU_LOGE("ERROR GET HttpGetter %s\r\n", _httpGetter->getErrorText());
        _httpGetter = nullptr;  // Ensure cleanup on error
        return false;
    }

    _httpGetter = nullptr;
    return true;
}

float Provider::read_http(String Url)
{
    auto oAcPower = _dataCurrent.get<DataPointLabel::AcPower>();
    HttpRequestConfig HttpRequest;
    JsonDocument jsonResponse;
    strlcpy(HttpRequest.Url, Url.c_str(), sizeof(HttpRequest.Url));
    HttpRequest.Timeout = 60;  // 60 seconds
    _httpGetter = std::make_unique<HttpGetter>(HttpRequest);
    DTU_LOGI("Start reading from URL: %s\r\n",Url.c_str());

    if (!_httpGetter->init()) {
        DTU_LOGE("ERROR INIT HttpGetter %s\r\n", _httpGetter->getErrorText());
        _httpGetter = nullptr;  // Ensure cleanup on error
        return *oAcPower;
    }

    _httpGetter->addHeader("Content-Type", "application/json");
    _httpGetter->addHeader("Accept", "application/json");

    auto res = _httpGetter->performGetRequest();
    if (!res) {
        DTU_LOGE("ERROR GET HttpGetter %s\r\n", _httpGetter->getErrorText());
        _httpGetter = nullptr;  // Ensure cleanup on error
        return *oAcPower;
    }

    auto pStream = res.getStream();
    if (!pStream) {
        DTU_LOGE("Programmer error: HTTP request yields no stream");
        _httpGetter = nullptr;  // Ensure cleanup on error
        return *oAcPower;
    }

    const DeserializationError error = deserializeJson(jsonResponse, *pStream);
    if (error) {
        String msg = error.c_str();
        DTU_LOGE("Unable to parse server response as JSON: %s\r\n", msg.c_str());
        _httpGetter = nullptr;  // Ensure cleanup on error
        return *oAcPower;
    }

    auto pathResolutionResult = Utils::getJsonValueByPath<float>(jsonResponse, uri_powerparam);
    if (!pathResolutionResult.second.isEmpty()) {
        DTU_LOGE("ERROR reading AC Power from Smart Plug %s\r\n",pathResolutionResult.second.c_str());
    }

    _httpGetter = nullptr;
    return pathResolutionResult.first;
}

void Provider::powerControlLoop()
{
    auto& config = Configuration.get();
    auto oAcPower = _dataCurrent.get<DataPointLabel::AcPower>();

    // ***********************
    // Emergency charge
    // ***********************
    auto stats = Battery.getStats();
    if (!_batteryEmergencyCharging && config.GridCharger.EmergencyChargeEnabled && stats->getImmediateChargingRequest()) {
        if (!oAcPower) {
            DTU_LOGW("Cannot perform emergency charging with unknown PSU max ac power value");
            return;
        }

        _batteryEmergencyCharging = true;

        DTU_LOGI("Emergency Charge AC Power %.02f", *oAcPower);
        PowerON();
        return;
    }

    if (_batteryEmergencyCharging && !stats->getImmediateChargingRequest()) {
        DTU_LOGI("Emergency Charge OFF %.02f", *oAcPower);
        PowerOFF();
        return;
    }
    // ***********************
    // Automatic power control
    // ***********************

    if (config.GridCharger.AutoPowerEnabled) {

        // Check if we should run automatic power calculation at all.
        // We may have set a value recently and still wait for output stabilization
        if (_autoModeBlockedTillMillis > millis()) {
             return;
        }

        if (PowerLimiter.isGovernedBatteryPoweredInverterProducing() && powerstate) {
            PowerOFF();
            powerstate=false;
            DTU_LOGI("Inverter is active, disable PSU");
            _autoModeBlockedTillMillis = millis() + 19900;
            return;
        }

        if (millis() - _dataCurrent.getLastUpdate() > 30 * 1000) {
            DTU_LOGW("Cannot perform auto power control when critical PSU values are outdated");
            _autoModeBlockedTillMillis = millis() + 6000;
            return;
        }

        if (!oAcPower ) {
            DTU_LOGW("Cannot perform auto power control while critical PSU values are still unknown");
            _autoModeBlockedTillMillis = millis() + 6000;
            return;
        }


        // Check whether the battery SoC limit setting is enabled
        if (config.Battery.Enabled && config.GridCharger.AutoPowerBatterySoCLimitsEnabled) {
            uint8_t _batterySoC = Battery.getStats()->getSoC();
            _autoPowerEnabled = true;
            // Power OFF charger if the BMS reported SoC reaches or exceeds the user configured value
            if (_batterySoC >= config.GridCharger.AutoPowerStopBatterySoCThreshold && powerstate) {
                DTU_LOGV("Current battery SoC %i reached stop threshold %i", _batterySoC, config.GridCharger.AutoPowerStopBatterySoCThreshold);
                PowerOFF();
                _autoPowerEnabled = false;
            }
            // Don't run auto mode some time to allow for output stabilization after issuing a new value
            _autoModeBlockedTillMillis = millis() + 19900;
        } else if (powerstate){
            _autoPowerEnabled = false;
            PowerOFF();
        }

        // We have received a new PowerMeter value. Also we're _autoPowerEnabled
        if (PowerMeter.getLastUpdate() > _lastPowerMeterUpdateReceivedMillis && _autoPowerEnabled) {
            _lastPowerMeterUpdateReceivedMillis = PowerMeter.getLastUpdate();
            float powerTotal = round(PowerMeter.getPowerTotal());
            DTU_LOGV("powerTotal: %.0fW, AC Power: %.0fW, Target to Power on Charger: %.0fW", powerTotal, *oAcPower, config.GridCharger.AutoPowerTargetPowerConsumption - maximumAcPower);
            if (powerTotal > config.GridCharger.AutoPowerTargetPowerConsumption && powerstate) {
                DTU_LOGI("Power consumption %.0fW exceeds target %.0fW, disable PSU", powerTotal, config.GridCharger.AutoPowerTargetPowerConsumption );
                PowerOFF();
                _autoModeBlockedTillMillis = millis() + 29900;
                return;
            }
            else if (powerTotal < config.GridCharger.AutoPowerTargetPowerConsumption - maximumAcPower && !powerstate) {
                DTU_LOGI("Set AC Power to %.0fW to reach target consumption of %.0fW (current total: %.0fW)", maximumAcPower, config.GridCharger.AutoPowerTargetPowerConsumption , powerTotal);
                PowerON();
                _autoModeBlockedTillMillis = millis() + 29900;
            }

        }
    }
}
void Provider::dataPollingLoopHelper(void* context)
{
    auto pInstance = static_cast<Provider*>(context);
    pInstance->dataPollingLoop();
    pInstance->_dataPollingTaskDone = true;
    vTaskDelete(nullptr);
}

void Provider::dataPollingLoop()
{
    std::unique_lock lock(_dataPollingMutex);

    while (!_stopPollingData) {
        auto elapsedMillis = millis() - _lastDataPoll;

        if (_lastDataPoll > 0 && elapsedMillis < DATA_POLLING_INTERVAL_MS) {
            auto sleepMs = DATA_POLLING_INTERVAL_MS - elapsedMillis;
            _dataPollingCv.wait_for(lock, std::chrono::milliseconds(sleepMs),
                    [this] { return _stopPollingData; }); // releases the mutex
            continue;
        }

        _lastDataPoll = millis();

        lock.unlock(); // polling can take quite some time
        pollData();
        lock.lock();
    }
}
void Provider::pollData()
{
    acPowerCurrent = read_http(uri_stats);
    DTU_LOGV("acPowerCurrent: %f", acPowerCurrent);
    if (acPowerCurrent > 0)
    {
        powerstate=true;
    }
    // Update data points
    {
        auto scopedLock = _dataCurrent.lock();
        _dataCurrent.add<DataPointLabel::AcPower>(acPowerCurrent);
    }
    _stats->updateFrom(_dataCurrent);
}

} // namespace GridChargers::HTTP
