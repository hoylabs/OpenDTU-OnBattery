// SPDX-License-Identifier: GPL-2.0-or-later

#include <gridcharger/trucki/Provider.h>
#include <gridcharger/trucki/DataPoints.h>
#include <battery/Controller.h>
#include <powermeter/Controller.h>
#include <PowerLimiter.h>
#include <Utils.h>
#include <WiFiUdp.h>
#include <LogHelper.h>

#undef TAG
static const char* TAG = "gridCharger";
static const char* SUBTAG = "Trucki";

static constexpr unsigned int udpPort = 4211;  // trucki T2xG port
static WiFiUDP TruckiUdp;

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

    if (!TruckiUdp.begin(udpPort)) {
        DTU_LOGE("Failed to initialize UDP");
        return false;
    }

    return true;
}

void Provider::deinit()
{
    // TODO(andreasboehm): probably something is not working correctly because we 'loose connection'
    // to the PSU after the configuration was changed without a reboot
    TruckiUdp.stop();

    _pollingTaskDone = false;

    std::unique_lock<std::mutex> pollingLock(_pollingMutex);
    _stopPolling = true;
    pollingLock.unlock();

    _cv.notify_all();

    if (_pollingTaskHandle != nullptr) {
        while (!_pollingTaskDone) { delay(10); }
        _pollingTaskHandle = nullptr;
    }

    _powerControlTaskDone = false;

    std::unique_lock<std::mutex> powerControlLock(_powerControlMutex);
    _stopPowerControl = true;
    powerControlLock.unlock();

    _powerControlCv.notify_all();

    if (_powerControlTaskHandle != nullptr) {
        while (!_powerControlTaskDone) { delay(10); }
        _powerControlTaskHandle = nullptr;
    }
}

void Provider::loop()
{
    generalPowerControlLoop();

    if (_pollingTaskHandle == nullptr) {
        std::unique_lock<std::mutex> lock(_pollingMutex);
        _stopPolling = false;
        lock.unlock();

        uint32_t constexpr stackSize = 4096;
        xTaskCreate(Provider::pollingLoopHelper, "TruckiPolling",
                stackSize, this, 1/*prio*/, &_pollingTaskHandle);
    }

    if (_powerControlTaskHandle == nullptr) {
        std::unique_lock<std::mutex> lock(_powerControlMutex);
        _stopPowerControl = false;
        lock.unlock();

        uint32_t constexpr stackSize = 4096;
        xTaskCreate(Provider::powerControlLoopHelper, "TruckiControl",
                stackSize, this, 1/*prio*/, &_powerControlTaskHandle);
    }
}

void Provider::generalPowerControlLoop()
{
    auto& config = Configuration.get();

    auto oMaxAcPower = _dataCurrent.get<DataPointLabel::MaxAcPower>();
    auto oOutputPower = _dataCurrent.get<DataPointLabel::DcPower>();

    // ***********************
    // Emergency charge
    // ***********************
    auto stats = Battery.getStats();
    if (!_batteryEmergencyCharging && config.GridCharger.EmergencyChargeEnabled && stats->getImmediateChargingRequest()) {
        if (!oMaxAcPower) {
            // TODO(andreasboehm): if this situation actually occurs, this message
            // will be printed with high frequency for a prolonged time. how can
            // we deal with that?
            DTU_LOGW("Cannot perform emergency charging with unknown PSU max ac power value");
            return;
        }

        _batteryEmergencyCharging = true;

        DTU_LOGI("Emergency Charge AC Power %.02f", *oMaxAcPower);
        setChargerPowerAc(*oMaxAcPower);
        return;
    }

    if (_batteryEmergencyCharging && !stats->getImmediateChargingRequest()) {
        // Battery request has changed. Set current to 0, wait for PSU to respond and then clear state
        // TODO(schlimmchen): this is repeated very often for up to (polling interval) seconds. maybe
        // trigger sending request for data immediately? otherwise implement a backoff instead.
        setChargerPowerAc(0);
        if (oOutputPower < 1) {
            _batteryEmergencyCharging = false;
        }
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

        if (PowerLimiter.isGovernedBatteryPoweredInverterProducing()) {
            setChargerPowerAc(0);
            _autoPowerEnabled = false;
            DTU_LOGI("Inverter is active, disable PSU");
            return;
        }

        auto oOutputVoltage = _dataCurrent.get<DataPointLabel::DcVoltage>();
        auto oMinAcPower = _dataCurrent.get<DataPointLabel::MinAcPower>();
        auto oOutputCurrent = _dataCurrent.get<DataPointLabel::DcCurrent>();

        // TODO(andreasboehm): check age of datapoints before using them
        if (!oOutputPower || !oOutputVoltage || !oMinAcPower || !oMaxAcPower || !oOutputCurrent) {
            DTU_LOGW("Cannot perform auto power control while critical PSU values are still unknown");
            _autoModeBlockedTillMillis = millis() + 1000;
            return;
        }

        // Re-enable automatic power control if the output voltage has dropped below threshold
        if (oOutputVoltage && *oOutputVoltage < config.GridCharger.AutoPowerEnableVoltageLimit ) {
            _autoPowerEnabled = true;
        }

        // We have received a new PowerMeter value. Also we're _autoPowerEnabled
        // So we're good to calculate a new limit
        if (PowerMeter.getLastUpdate() > _lastPowerMeterUpdateReceivedMillis && _autoPowerEnabled) {
            _lastPowerMeterUpdateReceivedMillis = PowerMeter.getLastUpdate();

            float powerTotal = round(PowerMeter.getPowerTotal());

            // Calculate new power limit
            float newPowerLimit = -1 * powerTotal;

            // Powerlimit is the current output power + permissable Grid consumption
            newPowerLimit += *oOutputPower + config.GridCharger.AutoPowerTargetPowerConsumption;

            DTU_LOGD("powerTotal: %.0f, outputPower: %.01f, newPowerLimit: %.0f", powerTotal, *oOutputPower, newPowerLimit);

            // Check whether the battery SoC limit setting is enabled
            if (config.Battery.Enabled && config.GridCharger.AutoPowerBatterySoCLimitsEnabled) {
                uint8_t _batterySoC = Battery.getStats()->getSoC();
                // Sets power limit to 0 if the BMS reported SoC reaches or exceeds the user configured value
                if (_batterySoC >= config.GridCharger.AutoPowerStopBatterySoCThreshold) {
                    newPowerLimit = 0;
                    DTU_LOGD("Current battery SoC %i reached stop threshold %i, set newPowerLimit to %f",
                            _batterySoC, config.GridCharger.AutoPowerStopBatterySoCThreshold, newPowerLimit);
                }
            }

            if (newPowerLimit >= *oMinAcPower) {
                // Limit power to maximum
                if (newPowerLimit > *oMaxAcPower) {
                    newPowerLimit = *oMaxAcPower;
                }

                auto efficiency = _dataCurrent.get<DataPointLabel::Efficiency>().value_or(90) / 100.0f;
                efficiency = efficiency > 0.5f ? efficiency : 0.9f;

                // Calculate output current
                float calculatedCurrent = efficiency * (newPowerLimit / *oOutputVoltage);

                // Limit output current to value requested by BMS
                float permissibleCurrent = stats->getChargeCurrentLimit() - (stats->getChargeCurrent() - *oOutputCurrent); // BMS current limit - current from other sources, e.g. Victron MPPT charger
                float outputCurrent = std::min(calculatedCurrent, permissibleCurrent);
                outputCurrent = outputCurrent > 0 ? outputCurrent : 0;

                // calculate new power limit based on output current
                newPowerLimit = (outputCurrent * *oOutputVoltage) / efficiency;

                _autoPowerEnabled = true;
                setChargerPowerAc(newPowerLimit);

                // Don't run auto mode some time to allow for output stabilization after issuing a new value
                _autoModeBlockedTillMillis = millis() + 4 * POLLING_INTERVAL_MS;
            } else {
                // requested PL is below minium. Set power to 0
                _autoPowerEnabled = false;
                setChargerPowerAc(0);
            }
        }
    }
}

void Provider::setChargerPowerAc(float powerAc)
{
    _requestedPowerAc = powerAc;
}

void Provider::powerControlLoopHelper(void* context)
{
    auto pInstance = static_cast<Provider*>(context);
    pInstance->powerControlLoop();
    pInstance->_powerControlTaskDone = true;
    vTaskDelete(nullptr);
}

void Provider::powerControlLoop()
{
    std::unique_lock<std::mutex> lock(_powerControlMutex);

    while (!_stopPowerControl) {
        auto elapsedMillis = millis() - _lastPowerControlRequestMillis;

        if (_lastPowerControlRequestMillis > 0 && elapsedMillis < POWER_CONTROL_INTERVAL_MS) {
            auto sleepMs = POWER_CONTROL_INTERVAL_MS - elapsedMillis;
            _powerControlCv.wait_for(lock, std::chrono::milliseconds(sleepMs),
                    [this] { return _stopPowerControl; }); // releases the mutex
            continue;
        }

        _lastPowerControlRequestMillis = millis();

        lock.unlock(); // power control can take quite some time
        sendPowerControlRequest();
        parsePowerControlResponse();
        lock.lock();
    }
}

void Provider::sendPowerControlRequest()
{
    auto& config = Configuration.get();

    if (!config.GridCharger.AutoPowerEnabled && !_batteryEmergencyCharging) {
        return;
    }

    DTU_LOGI("Setting charging power to %.02fW AC", _requestedPowerAc);

    uint16_t acPowerSetpoint = _requestedPowerAc * 10; // ac power in W*10

    TruckiUdp.beginPacket(config.GridCharger.Trucki.IpAddress, udpPort);
    TruckiUdp.print(String(acPowerSetpoint));
    TruckiUdp.endPacket();
}

void Provider::parsePowerControlResponse()
{
    int packetSize = TruckiUdp.parsePacket();
    if (0 == packetSize) { return; }

    // Expected packet format: "1000;1000;1" (~14 chars max)
    // - first number is the current AC power in W*10
    // - second number is the max AC power in W*10
    // - third number is the battery state and optional, not available in older versions
    // Using 64 bytes for safety margin and UDP packet alignment
    static constexpr size_t kMaxPacketSize = 64;
    char packet[kMaxPacketSize];
    int readBytes = TruckiUdp.read(packet, std::min(static_cast<size_t>(packetSize), kMaxPacketSize));
    if (0 == readBytes) { return; }

    // current AC power (W*10)
    char *saveptr;
    char *token = strtok_r(packet, ";", &saveptr);
    char ac_str[6];
    strncpy(ac_str, token, sizeof(ac_str));
    ac_str[sizeof(ac_str) - 1] = '\0';
    float acPowerCurrent = atoi(ac_str) / 10.0f;

    // max AC power (W*10)
    token = strtok_r(NULL, ";", &saveptr);
    char limit_str[6];
    if (token != 0) {
        strncpy(limit_str, token, sizeof(limit_str));
    } else {
        strncpy(limit_str, "0", sizeof(limit_str));
    }
    limit_str[sizeof(limit_str) - 1] = '\0';
    float acPowerMax = atoi(limit_str) / 10.0f;

    DTU_LOGV("acPowerCurrent: %f, acPowerMax: %f", acPowerCurrent, acPowerMax);

    // Update data points
    {
        auto scopedLock = _dataCurrent.lock();
        _dataCurrent.add<DataPointLabel::AcPower>(acPowerCurrent);
        _dataCurrent.add<DataPointLabel::MaxAcPower>(acPowerMax);
    }

    _stats->updateFrom(_dataCurrent);
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
