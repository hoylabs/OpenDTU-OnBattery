// SPDX-License-Identifier: GPL-2.0-or-later

/* Battery-Guard
 *
 * The Battery-Guard has several features.
 * - Calculate the battery open circuit voltage
 * - Calculate the battery internal resistance
 * - Limit the power drawn from the battery, if the battery voltage is close to the stop threshold.
 * - Periodically recharge the battery to 100% SoC (draft)
 *
 * Basic principe of the feature: "Battery open circuit voltage"
 * As soon as we kow the battery internal resistance we calculate the open circuit voltage.
 * open circuit voltage = battery voltage - battery current * resistance.
 *
 * Basic principe of the feature: "Battery internal resistance"
 * In general, we try to use steady state values to calculate the battery internal resistance.
 * We always search for 2 consecutive values and build an average for voltage and current.
 * First, we search for a start value.
 * Second, we search for a sufficient current change (trigger).
 * Third, we search for min and max values in a time frame of 15 seconds
 * After the time, we calculate the resistance from the min and max difference.
 * Note: We need high load changes to get sufficient calculation results. About 100W on 24VDC or 180W on 48VDC.
 * The resistance on LifePO4 batteries is not a fixed value, it depends from temperature, charge and time
 * after a load change.
 *
 * Basic principe of the function: "Low voltage limiter"
 * If the battery voltage is close to the stop threshold, the battery limiter will calculate a maximum power limit
 * to keep the battery voltage above the voltage threshold.
 * The inverter is only switched-off when the threshold is exceeded and the inverter output cannot be reduced any further.
 *
 * Basic principe of the function: "Periodically recharge the battery"
 * After some days we start to reduce barriers, to make it more easier for the sun to fully charge the battery.
 * When we reach 100% SoC we remove all restrictions and start a new period.
 * Especially usefull during winter to support the SoC calibration of the BMS
 *
 * Notes:
 * Some function are still under development.
 * These functions were developed for the battery provider "Smart Shunt", but should also work with other providers
 *
 * 01.08.2024 - 0.1 - first version. "Low voltage power limiter"
 * 09.12.2024 - 0.2 - add of function "Periodically recharge the battery"
 * 11.12.2024 - 0.3 - add of function "Battery internal resistance" and "Open circuit voltage"
 * 14.02.2025 - 0.4 - works now with all battery providers, accept different time stamps for voltage and current
 */

#include <frozen/map.h>
#include <battery/Controller.h>
#include <solarcharger/Controller.h>
#include <Configuration.h>
#include <BatteryGuard.h>
#include <LogHelper.h>

static const char* TAG = "battery";
static const char* SUBTAG = "Battery Guard";

BatteryGuardClass BatteryGuard;

/*
 * Initialize the battery guard
 */
void BatteryGuardClass::init(Scheduler& scheduler) {
    // init the fast loop
    scheduler.addTask(_fastLoopTask);
    _fastLoopTask.setCallback(std::bind(&BatteryGuardClass::loop, this));
    _fastLoopTask.setIterations(TASK_FOREVER);
    _fastLoopTask.enable();

    // init the slow loop
    scheduler.addTask(_slowLoopTask);
    _slowLoopTask.setCallback(std::bind(&BatteryGuardClass::slowLoop, this));
    _slowLoopTask.setIterations(TASK_FOREVER);
    _slowLoopTask.setInterval(60*1000);
    _slowLoopTask.enable();

    _analyzedResolutionV = 0.10f;
    _analyzedResolutionI = 1.0f;
    _analyzedPeriod.reset(10000);
    _analyzedUIDelay.reset(5000);
    updateSettings();
}


/*
 * Update some settings of the battery guard
 */
void BatteryGuardClass::updateSettings(void) {
    const auto& config = Configuration.get();

    // Check if BatteryGuard is enabled and battery provider is configured
    _useBatteryGuard = config.BatteryGuard.Enabled && config.Battery.Enabled;
    _resistanceFromConfig = config.BatteryGuard.InternalResistance / 1000.0f; // mOhm -> Ohm
}

/*
 * Normal loop, fetches the battery values (voltage, current and measurement time stamp) from the battery provider
 */
void BatteryGuardClass::loop(void) {
    const auto& config = Configuration.get();

    if (!_useBatteryGuard
        || !config.Battery.Enabled) {
        // not active or no battery, we abort
        return;
    }

    if (!Battery.getStats()->isVoltageValid()
        || !Battery.getStats()->isCurrentValid()) {
        // stats not valid, we abort
        return;
    }

    auto const& u2Value = Battery.getStats()->getVoltage();
    auto const& u2Time = Battery.getStats()->getLastVoltageUpdate();
    auto const& i2Value = Battery.getStats()->getChargeCurrent();
    auto const& i2Time = Battery.getStats()->getLastCurrentUpdate();

    if ( (_u1Data.timeStamp == u2Time) && (_i1Data.timeStamp == i2Time) ) { return; } // same time stamp again, we abort

    if (u2Time == i2Time) {
        // the simple handling: voltage und current have the same time stamp
        _i1Data = { i2Value, i2Time, true };
        _u1Data = { u2Value, u2Time, true };
        _analyzedUIDelay.addNumber(0.0f);
        updateBatteryValues(u2Value, i2Value, i2Time);
        return;
    }

    // the special handling: voltage and current time stamp are different
    // Note: In worst case, this will add a delay of 1 measurement period
    if (i2Time != _i1Data.timeStamp) {

        // check if new U1 data is available and check if we have to use U1 or U2
        Data uxData = _u1Data;
        if ((u2Time != _u1Data.timeStamp) && ((millis() - u2Time) > (millis() - i2Time))) {
            uxData = { u2Value, u2Time, true };
        }

        if (uxData.valid && _i1Data.valid) {

            // check if Ux time stamp is closer to I1 or I2 time stamp
            if ((i2Time - uxData.timeStamp) < (uxData.timeStamp - _i1Data.timeStamp)) {
                _analyzedUIDelay.addNumber(static_cast<float>(i2Time - uxData.timeStamp));
                updateBatteryValues(uxData.value, i2Value, uxData.timeStamp); // we use the older time stamp
            } else {
                _analyzedUIDelay.addNumber(-static_cast<float>(uxData.timeStamp - _i1Data.timeStamp));
                updateBatteryValues(uxData.value, _i1Data.value, _i1Data.timeStamp); // we use the older time stamp
            }
            _u1Data.valid = false;
        }

        _i1Data = { i2Value, i2Time, true }; // store the next I1 data
    }

    if (u2Time != _u1Data.timeStamp) { _u1Data = { u2Value, u2Time, true }; } // store the next U1 data
}


/*
 * Slow periodical tasks, will be called once a minute
 */
void BatteryGuardClass::slowLoop(void) {
    const auto& config = Configuration.get();
    if (!_useBatteryGuard
        || !config.Battery.Enabled
        || !DTU_LOG_IS_VERBOSE) {
        // not active or no battery or no verbose logging, we abort
        return;
    }

    DTU_LOGV("");
    DTU_LOGV("------------- Battery Guard Report (every minute) -------------");
    DTU_LOGV("");

    // "Open circuit voltage"
    printOpenCircuitVoltageReport();

    DTU_LOGV("-----------------------------------------------------------");
    DTU_LOGV("");
}


/*
 * Update the battery guard with new values. (voltage[V], current[A], millisStamp[ms])
 * Note: Just call the function if new values are available. Current into the battery must be positive.
 */
void BatteryGuardClass::updateBatteryValues(float const volt, float const current, uint32_t const millisStamp) {
    if (volt <= 0.0f) { return; }

    // analyse the measurement period
    if ((_battMillis != 0) && (millisStamp > _battMillis)) {
        _analyzedPeriod.addNumber(millisStamp - _battMillis);
    }

    // analyse the voltage and current resolution
    auto resolution = std::abs(volt - _battVoltage);
    if ((resolution >= 0.001f) && (resolution < _analyzedResolutionV)) { _analyzedResolutionV = resolution; }
    resolution = std::abs(current - _battCurrent);
    if ((resolution >= 0.001f) && (resolution < _analyzedResolutionI)) { _analyzedResolutionI = resolution; }

    // store the values
    _battMillis = millisStamp;
    _battVoltage = volt;
    _battCurrent = current;
    _battVoltageAVG.addNumber(_battVoltage);
    calculateInternalResistance(_battVoltage, _battCurrent);
    calculateOpenCircuitVoltage(_battVoltage, _battCurrent);
}


/*
 * Calculate the battery open circuit voltage.
 */
void BatteryGuardClass::calculateOpenCircuitVoltage(float const nowVoltage, float const nowCurrent) {
    // resistance must be available and current flow into the battery must be positive
    auto oResistor = getInternalResistance();
    if (oResistor.has_value()) {
        _openCircuitVoltageAVG.addNumber(nowVoltage - nowCurrent * oResistor.value());
    }
}


/*
 * The battery internal resistance, calculated or configured or nullopt if neither value is valid
 */
std::optional<float> BatteryGuardClass::getInternalResistance(void) const {
    // we use the calculated value if we have 5 calculations minimum
    if (_useBatteryGuard) {
        if (_resistanceFromCalcAVG.getCounts() >= MINIMUM_RESISTANCE_CALC) { return _resistanceFromCalcAVG.getAverage(); }
        if (_resistanceFromConfig != 0.0f) { return _resistanceFromConfig; }
    }
    return std::nullopt;
}


/*
 * True if we use the calculated resistor and not the configured one
 */
bool BatteryGuardClass::isInternalResistanceCalculated(void) const {
    return (_resistanceFromCalcAVG.getCounts() >= MINIMUM_RESISTANCE_CALC);
}


/*
 * The battery open circuit voltage or nullopt if value is not valid or not enabled
 */
std::optional<float> BatteryGuardClass::getOpenCircuitVoltage(void) {
    if ((_useBatteryGuard && _openCircuitVoltageAVG.getCounts() > 0) && isDataValid()) {
        return _openCircuitVoltageAVG.getAverage();
    }
    _notAvailableCounter++;
    return std::nullopt;
}


/*
 * True if the measurement resolution, period and time delay between voltage and current is sufficient
 * Requirement: Voltage <= 20mV, Current <= 200mA, Period <= 4s, Time delay <= 1s
 */
bool BatteryGuardClass::isResolutionOK(void) const {
    return (_analyzedResolutionV <= MAXIMUM_VOLTAGE_RESOLUTION)
        && (_analyzedResolutionI <= MAXIMUM_CURRENT_RESOLUTION)
        && (_analyzedPeriod.getAverage() <= MAXIMUM_MEASUREMENT_TIME_PERIOD)
        && (std::abs(_analyzedUIDelay.getAverage()) <= MAXIMUM_V_I_TIME_STAMP_DELAY);
}


/*
 * Calculate the battery DC-Pulse-Resistance based on the voltage measurement position of the battery provider.
 * Note: The calculation will not work, if the performance of the battery provider is not good enough
 * or if the power difference is not big enough or too slow.
 */
void BatteryGuardClass::calculateInternalResistance(float const nowVoltage, float const nowCurrent) {

    // lambda function to avoid nested if-else statements and code doubleing
    auto cleanExit = [&](RState state) -> void {
        if (_rStateLast == state) { return; } // no change, we abort without logging
        _rStateLast = state;
        DTU_LOGV("Resistance calculation state: %s", getResistanceStateText(state).data());
        if (state > _rStateMax) { _rStateMax = state; }
    };

    // check the resolution and the calculation frequency
    if (!isResolutionOK()) { return cleanExit(RState::RESOLUTION); }
    if ((millis() - _lastDataInMillis) < 900 ) { return cleanExit(RState::TIME); }
    _lastDataInMillis = millis();
    if (!_minMaxAvailable) { _rState = RState::IDLE; }

    // Check if we are in a SoC range that makes sense for the resistance calculation
    auto const& actualSoC = Battery.getStats()->getSoC();
    if ((actualSoC <= 15.0f) || actualSoC >= 90.0f) { return cleanExit(RState::SOC_RANGE); }

    // check for the trigger event (sufficient current change)
    auto const minDiffCurrent = 4.0f; // seems to be a good value for all battery providers
    if (!_triggerEvent && _minMaxAvailable && (std::abs(nowCurrent - _pMinVolt.second) > (minDiffCurrent/2.0f))) {
        _lastTriggerMillis = millis();
        _triggerEvent = true;
        _rState = RState::TRIGGER;
    }

    // we evaluate min and max values in a time duration of 15 sec after the trigger event
    if (!_triggerEvent || (_triggerEvent && (millis() - _lastTriggerMillis) < 15*1000)) {

        // we use the measurement resolution to decide if two consecutive values are almost identical
        auto minVoltage = (_triggerEvent) ? 0.2f : std::max(_analyzedResolutionV * 3.0f, 0.01f);
        auto minCurrent = std::max(_analyzedResolutionI * 3.0f, 0.2f);

        // after the first-pair-after-the-trigger, we check if the current is stable
        // if the current is not stable we break the calculation because we have again a power transition
        // which influences the quality of the calculation
        if (_pairAfterTriggerAvailable && (std::abs(_checkCurrent - nowCurrent) > minCurrent)) {
            _firstOfTwoAvailable = false;
            _minMaxAvailable = false;
            _triggerEvent = false;
            _pairAfterTriggerAvailable = false;
            return cleanExit(RState::SECOND_BREAK);
        }

        // we must avoid to use measurement values during any power transitions
        // to solve this problem, we check whether two consecutive measurements are almost identical
        if (_firstOfTwoAvailable && (std::abs(_pFirstVolt.first - nowVoltage) <= minVoltage) &&
            (std::abs(_pFirstVolt.second - nowCurrent) <= minCurrent)) {

        auto avgVolt = std::make_pair((nowVoltage+_pFirstVolt.first)/2.0f, (nowCurrent+_pFirstVolt.second)/2.0f);
        if (!_minMaxAvailable || !_triggerEvent) {
            _pMinVolt = _pMaxVolt = avgVolt;
            _minMaxAvailable = true;
            _rState = RState::FIRST_PAIR; // we have the first pair (before the trigger event)
        } else {
            if (avgVolt.first < _pMinVolt.first) { _pMinVolt = avgVolt; }
            if (avgVolt.first > _pMaxVolt.first) { _pMaxVolt = avgVolt; }
                _pairAfterTriggerAvailable = true;
                _checkCurrent = nowCurrent;
                _rState = RState::SECOND_PAIR; // we have the second pair (after the trigger event)
            }
        }
        _pFirstVolt = { nowVoltage, nowCurrent }; // preparation for the next two consecutive values
        _firstOfTwoAvailable = true;
        return cleanExit(_rState);
    }

    // reset conditions for the next calculation
    _firstOfTwoAvailable = false;
    _minMaxAvailable = false;
    _triggerEvent = false;
    _pairAfterTriggerAvailable = false;

    // now we have minimum and maximum values and we can try to calculate the resistance
    // we need a minimum power difference to get a sufficiently good result (failure < 20%)
    // SmartShunt: 40mV and 4A (about 100W on VDC=24V, Ri=12mOhm)
    auto minDiffVoltage = std::max(_analyzedResolutionV * 5.0f, 0.04f);
    auto diffVolt = _pMaxVolt.first - _pMinVolt.first;
    auto diffCurrent = std::abs(_pMaxVolt.second - _pMinVolt.second);   // can be negative
    if ((diffVolt >= minDiffVoltage) && (diffCurrent >= minDiffCurrent)) {
        float resistor = diffVolt / diffCurrent;
        auto reference = (_resistanceFromConfig != 0.0f) ?_resistanceFromConfig : _resistanceFromCalcAVG.getAverage();
        if ((reference != 0.0f) && ((resistor > reference * 2.0f) || (resistor < reference / 2.0f))) {
            _rState = RState::TOO_BAD; // safety feature: we try to keep out bad values from the average
        } else {
            _resistanceFromCalcAVG.addNumber(resistor);
            _rState = RState::CALCULATED;
        }
    } else {
        _rState = RState::DELTA_POWER;
    }

    return cleanExit(_rState);
}


/*
 * Prints the "Battery open circuit voltage" report
 */
void BatteryGuardClass::printOpenCircuitVoltageReport(void)
{
    DTU_LOGV("1) Open circuit voltage calculation. Battery data %s", (isResolutionOK()) ? "sufficient" : "not sufficient");
    DTU_LOGV("Open circuit voltage: %0.3fV (Actual battery voltage: %0.3fV)", _openCircuitVoltageAVG.getAverage(), _battVoltage);

    auto oResistance = getInternalResistance();
    if (!oResistance.has_value()) {
        DTU_LOGV("Resistance neither calculated (5 times) nor configured");
    } else {
        auto resCalc = (_resistanceFromCalcAVG.getCounts() > 4) ? _resistanceFromCalcAVG.getAverage() * 1000.0f : 0.0f;
        DTU_LOGV("Resistance in use: %0.1fmOhm (Calc.: %0.1fmOhm, Config.: %0.1fmOhm)",
            oResistance.value() * 1000.0f, resCalc, _resistanceFromConfig * 1000.0f);
    }

    DTU_LOGV("Resistance calc.: %0.1fmOhm (Min: %0.1f, Max: %0.1f, Amount: %i)",
        _resistanceFromCalcAVG.getAverage()*1000.0f, _resistanceFromCalcAVG.getMin()*1000.0f,
        _resistanceFromCalcAVG.getMax()*1000.0f, _resistanceFromCalcAVG.getCounts());

    DTU_LOGV("Resistance calculation state: %s", getResistanceStateText(_rStateMax).data());

    DTU_LOGV("Voltage resolution: %0.0fmV, Current resolution: %0.0fmA",
        _analyzedResolutionV * 1000.0f, _analyzedResolutionI * 1000.0f);

    DTU_LOGV("Measurement period: %0.0fms, V-I time stamp delay: %0.0fms",
        _analyzedPeriod.getAverage(), _analyzedUIDelay.getAverage());

    DTU_LOGV("Open circuit voltage not available counter: %i", _notAvailableCounter);
}


/*
 * Returns a string according to resistance calculation state
 */
frozen::string const& BatteryGuardClass::getResistanceStateText(BatteryGuardClass::RState tNr) const
{
    static const frozen::string missing = "programmer error: missing status text";

    static const frozen::map<RState, frozen::string, 11> texts = {
        { RState::IDLE, "Idle" },
        { RState::RESOLUTION, "Battery data insufficient" },
        { RState::SOC_RANGE, "SoC out of range 10%-90%" },
        { RState::TIME, "Measurement time to fast" },
        { RState::FIRST_PAIR, "Start data available" },
        { RState::TRIGGER, "Trigger event" },
        { RState::SECOND_PAIR, "Collecting data after trigger" },
        { RState::SECOND_BREAK, "Second power change after trigger" },
        { RState::DELTA_POWER, "Power difference not high enough" },
        { RState::TOO_BAD, "Resistance out of safety range" },
        { RState::CALCULATED, "Resistance calculated" }
    };

    auto iter = texts.find(tNr);
    if (iter == texts.end()) { return missing; }

    return iter->second;
}
