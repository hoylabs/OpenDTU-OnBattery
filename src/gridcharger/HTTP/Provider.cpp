// SPDX-License-Identifier: GPL-2.0-or-later

#include <gridcharger/HTTP/Provider.h>
#include <battery/Controller.h>
#include <powermeter/Controller.h>
#include <PowerLimiter.h>
#include <LogHelper.h>
#include <array>

#undef TAG
static const char* TAG = "gridCharger";
static const char* SUBTAG = "HTTP";

namespace GridChargers::HTTP {

bool Provider::init()
{
    DTU_LOGI("Initialize HTTP AC charger interface...");

    auto const& config = Configuration.get().GridCharger.HTTP;

    _chargers.clear();
    for (size_t i = 0; i < GRIDCHARGER_HTTP_MAX_CHARGERS; ++i) {
        if (!config.Chargers[i].Enabled) { continue; }

        auto charger = std::make_shared<Charger>(i, config);
        if (!charger->init()) { return false; }
        _chargers.push_back(std::move(charger));
    }

    if (_chargers.empty()) {
        DTU_LOGE("No HTTP AC charger enabled");
        return false;
    }

    _stats->setChargers(std::vector<std::shared_ptr<Charger const>>(_chargers.cbegin(), _chargers.cend()));

    _stopPollingData = false;
    return true;
}

void Provider::deinit()
{
    DTU_LOGI("DEInitialize HTTP AC charger interface...");

    std::unique_lock pollingLock(_dataPollingMutex);
    _stopPollingData = true;
    pollingLock.unlock();

    _dataPollingCv.notify_all();

    if (_dataPollingTaskHandle != nullptr) {
        while (!_dataPollingTaskDone) { delay(10); }
        _dataPollingTaskHandle = nullptr;
    }
    _dataPollingTaskDone = false;

    // the actual state of the smart plugs is unknown, so switch off all of them
    for (auto& charger : _chargers) { charger->powerOff(); }
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
    }
}

bool Provider::getAutoPowerStatus() const
{
    // battery-powered inverters have priority over the HTTP chargers: they are
    // never blocked by a running charger. instead, all chargers are switched
    // off as soon as an inverter produces (see powerControlLoop()).
    return false;
}

bool Provider::isAnyChargerOn() const
{
    for (auto const& charger : _chargers) {
        if (charger->getPowerState()) { return true; }
    }
    return false;
}

void Provider::switchAll(bool on)
{
    for (auto& charger : _chargers) {
        if (charger->getPowerState() == on) { continue; }
        on ? charger->powerOn() : charger->powerOff();
    }
}

void Provider::powerControlLoop()
{
    auto const& config = Configuration.get();
    auto stats = Battery.getStats();
    uint8_t batterySoC = stats->getSoC();
    if (millis() - _lastSoCLogMillis > 10000) {
        DTU_LOGI("SoC: %i", batterySoC);
        _lastSoCLogMillis = millis();
    }

    auto blockAutoMode = [this](uint32_t ms) {
        _autoModeBlockedSinceMillis = millis();
        _autoModeBlockedForMillis = ms;
    };

    // ***********************
    // Emergency charge
    // ***********************
    if (!_batteryEmergencyCharging && config.GridCharger.EmergencyChargeEnabled && stats->getImmediateChargingRequest()) {
        _batteryEmergencyCharging = true;
        DTU_LOGI("Emergency charge ON, SoC: %i", batterySoC);
        switchAll(true);
        return;
    }

    if (_batteryEmergencyCharging) {
        if (!stats->getImmediateChargingRequest()) {
            DTU_LOGI("Emergency charge OFF, SoC: %i", batterySoC);
            switchAll(false);
            _batteryEmergencyCharging = false;
            blockAutoMode(STABILIZATION_TIME_MS);
        }
        return;
    }

    // ***********************
    // Automatic power control
    // ***********************
    if (!config.GridCharger.AutoPowerEnabled) { return; }

    // the following conditions switch off all chargers immediately,
    // regardless of the stabilization time and the minimum on/off time.
    if (!config.Battery.Enabled ||
        PowerLimiter.isGovernedBatteryPoweredInverterProducing() ||
        PowerLimiter.isFullSolarPassthroughActive()) {
        if (isAnyChargerOn()) {
            DTU_LOGI("Inverter is active, full solar passthrough or battery disabled, disable all chargers");
            switchAll(false);
            blockAutoMode(STABILIZATION_TIME_MS);
        }
        return;
    }

    if (config.GridCharger.AutoPowerBatterySoCLimitsEnabled) {
        // Optionally power OFF all chargers if the BMS reported SoC reaches or exceeds the user configured value
        if (batterySoC >= config.GridCharger.AutoPowerStopBatterySoCThreshold) {
            if (isAnyChargerOn()) {
                DTU_LOGI("Current battery SoC %i reached stop threshold %i, disable all chargers",
                        batterySoC, config.GridCharger.AutoPowerStopBatterySoCThreshold);
                switchAll(false);
            }
            return;
        }

        if (isBatteryVoltageStopActive()) {
            if (isAnyChargerOn()) { switchAll(false); }
            return;
        }
    } else {
        _batteryVoltageStopActive = false;
    }

    // We may have switched recently and still wait for output stabilization
    if (millis() - _autoModeBlockedSinceMillis < _autoModeBlockedForMillis) { return; }

    // only act on new power meter values
    if (PowerMeter.getLastUpdate() <= _lastPowerMeterUpdateReceivedMillis) { return; }
    _lastPowerMeterUpdateReceivedMillis = PowerMeter.getLastUpdate();

    float powerTotal = round(PowerMeter.getPowerTotal());
    if (applyBestCombination(powerTotal, config.GridCharger.AutoPowerTargetPowerConsumption)) {
        blockAutoMode(STABILIZATION_TIME_MS);
    }
}

bool Provider::isBatteryVoltageStopActive()
{
    auto const& config = Configuration.get().GridCharger;
    float stopVoltage = config.AutoPowerStopBatteryVoltageThreshold;
    float resumeVoltage = config.AutoPowerResumeBatteryVoltageThreshold;

    if (stopVoltage <= 0) {
        _batteryVoltageStopActive = false;
        return false;
    }

    // keep the current decision while the battery voltage is unknown or outdated
    auto stats = Battery.getStats();
    if (!stats->isVoltageValid() || stats->getVoltageAgeSeconds() > 60) {
        return _batteryVoltageStopActive;
    }

    // the battery voltage drops as soon as charging stops, so charging only
    // resumes after the voltage dropped below the (lower) resume threshold.
    float voltage = stats->getVoltage();
    if (!_batteryVoltageStopActive && voltage >= stopVoltage) {
        DTU_LOGI("Battery voltage %.2fV reached stop threshold %.2fV, disable all chargers",
                voltage, stopVoltage);
        _batteryVoltageStopActive = true;
    } else if (_batteryVoltageStopActive && voltage < resumeVoltage) {
        DTU_LOGI("Battery voltage %.2fV dropped below resume threshold %.2fV, chargers may be enabled again",
                voltage, resumeVoltage);
        _batteryVoltageStopActive = false;
    }

    return _batteryVoltageStopActive;
}

bool Provider::applyBestCombination(float powerTotal, float targetConsumption)
{
    size_t const count = _chargers.size();

    // estimated household consumption without any charger, the expected
    // load of each charger and the chargers which must not be switched now.
    float baseConsumption = powerTotal;
    std::array<float, GRIDCHARGER_HTTP_MAX_CHARGERS> load{};
    uint32_t currentMask = 0;
    uint32_t lockedMask = 0;

    for (size_t i = 0; i < count; ++i) {
        auto const& charger = *_chargers[i];
        auto oPower = charger.getAcPower();

        if (charger.getPowerState()) {
            currentMask |= (1u << i);
            load[i] = oPower.value_or(charger.getNominalPower());
            baseConsumption -= load[i];
        } else {
            load[i] = charger.getNominalPower();
        }

        if (!oPower) {
            DTU_LOGD("[%s] Data outdated, keeping charger %s",
                    charger.getName().c_str(), charger.getPowerState() ? "on" : "off");
            lockedMask |= (1u << i);
        } else if (!charger.isSwitchAllowed()) {
            lockedMask |= (1u << i);
        }
    }

    auto loadOf = [&](uint32_t mask) {
        float sum = 0;
        for (size_t i = 0; i < count; ++i) {
            if (mask & (1u << i)) { sum += load[i]; }
        }
        return sum;
    };

    // try all combinations (at most 2^GRIDCHARGER_HTTP_MAX_CHARGERS). prefer
    // combinations which keep the consumption at or below the target and use
    // as much power as possible. if no combination achieves that, minimize the
    // consumption. on equal load, prefer the one with fewer switch operations.
    uint32_t bestMask = currentMask;
    float bestLoad = loadOf(currentMask);
    bool bestFeasible = baseConsumption + bestLoad <= targetConsumption;
    int bestSwitches = 0;

    for (uint32_t mask = 0; mask < (1u << count); ++mask) {
        if ((mask ^ currentMask) & lockedMask) { continue; }

        float l = loadOf(mask);
        bool feasible = baseConsumption + l <= targetConsumption;
        int switches = __builtin_popcount(mask ^ currentMask);

        bool better = false;
        if (feasible != bestFeasible) {
            better = feasible;
        } else if (l != bestLoad) {
            better = feasible ? (l > bestLoad) : (l < bestLoad);
        } else {
            better = switches < bestSwitches;
        }

        if (better) {
            bestMask = mask;
            bestLoad = l;
            bestFeasible = feasible;
            bestSwitches = switches;
        }
    }

    if (bestMask == currentMask) { return false; }

    // avoid switching back and forth for a negligible gain
    float currentLoad = loadOf(currentMask);
    bool currentFeasible = baseConsumption + currentLoad <= targetConsumption;
    if (currentFeasible && bestLoad - currentLoad < SWITCH_HYSTERESIS_W) { return false; }

    DTU_LOGI("Power consumption %.0fW, target %.0fW, base %.0fW: switching chargers from %.0fW to %.0fW",
            powerTotal, targetConsumption, baseConsumption, currentLoad, bestLoad);

    // switch off first to avoid a temporary peak in grid consumption
    for (size_t i = 0; i < count; ++i) {
        uint32_t bit = 1u << i;
        if ((currentMask & bit) && !(bestMask & bit)) { _chargers[i]->powerOff(); }
    }
    for (size_t i = 0; i < count; ++i) {
        uint32_t bit = 1u << i;
        if (!(currentMask & bit) && (bestMask & bit)) { _chargers[i]->powerOn(); }
    }

    return true;
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
    for (auto& charger : _chargers) {
        {
            std::lock_guard<std::mutex> lock(_dataPollingMutex);
            if (_stopPollingData) { return; }
        }
        charger->poll();
    }
}

} // namespace GridChargers::HTTP
