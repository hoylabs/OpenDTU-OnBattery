// SPDX-License-Identifier: GPL-2.0-or-later

/* Battery-Guard
 *
 * The Battery-Guard has several features.
 * - Calculate the battery open circuit voltage
 * - Calculate the battery internal resistance
 * - Limit the power drawn from the battery, if the battery voltage is close to the stop threshold. (draft)
 * - Periodically recharge the battery to 100% SoC (draft)
 *
 * Basic principe of the feature: "Battery open circuit voltage"
 * As soon as we kow the battery internal resistance we calculate the open circuit voltage.
 * open circuit voltage = battery voltage - battery current * resistance.
 *
 * Basic principe of the feature: "Battery internal resistance"
 * In general, we try to use steady state values to calculate the battery internal resistance.
 * We always search for 2 consecutive values and build an average for voltage and current.
 * First, we surge for a start value.
 * Second, for a sufficient current change (trigger).
 * Third, for min and max values in a time frame of 15 seconds
 * After the time we calculate the resistance from the min and max difference.
 * Note: We need high load changes to get sufficient calculation results. About 100W on 24VDC or 180W on 48VDC.
 * The resistance on LifePO4 batteries is not a fixed value, he depends from temperature, charge and time
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
 * 14.02.2025 - 0.4 - works now with all battery providers
 */

#include <frozen/map.h>
#include "Configuration.h"
#include "MessageOutput.h"
#include "Battery.h"
#include "BatteryGuard.h"


#define INVERTER_EFF 0.95f          // inverter efficiency
#define TARGET_RANGE_V 0.01f        // target range of limit regulation
#define STOP_REASON_NO 0            // no stop up to now
#define STOP_REASON_NORMAL 1        // normal below the power limit stop
#define STOP_REASON_EMERGENCY 2     // emergency stop

// activate the following line to enable debug logging
#define DEBUG_LOGGING

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

    _determinedResolutionV = 1.0f;
    _determinedResolutionI = 1.0f;
    _determinedPeriod.reset(5000);
    updateSettings();
}


/*
 * Update some settings of the battery guard
 */
void BatteryGuardClass::updateSettings(void) {

    // todo: get values from the configuration file / web interface
    _useBatteryGuard = true;            // enable the battery guard
    _verboseLogging = false;            // enable verbose logging
    _verboseReport = true;              // enable verbose report

    // used for "Open circuit voltage"
    _resistanceFromConfig = 0;          // start value for the internal battery resistance [Ohm]

    // used for "Low voltage power limiter"
    _useLowVoltageLimiter = false;      // enable the low voltage power limiter

}


/*
 * Main loop, fetches the battery values (voltage, current and measurement time stemp) from the battery provider
 */
void BatteryGuardClass::loop(void) {
    if (!_useBatteryGuard) { return; }
    if (Battery.getStats()->getLastUpdate() == _lastUpdate) { return; } // millis rollover safe

    // new values are available
    _lastUpdate = Battery.getStats()->getLastUpdate();
    if (Battery.getStats()->isVoltageValid() && Battery.getStats()->isCurrentValid()) {
        updateBatteryValues(Battery.getStats()->getVoltage(), Battery.getStats()->getChargeCurrent(), _lastUpdate);
    }
}


/*
 * Slow periodical tasks, will be called once a minute
 */
void BatteryGuardClass::slowLoop(void) {
    if (!_useBatteryGuard) { return; }

    if (_verboseReport) {
        MessageOutput.printf("%s\r\n", getText(Text::T_HEAD).data());
        MessageOutput.printf("%s --------------------- Battery Guard Report (every minute) ----------------------\r\n",
            getText(Text::T_HEAD).data());
        MessageOutput.printf("%s Battery Guard: %s\r\n",
        getText(Text::T_HEAD).data(), (_useBatteryGuard) ? "Enabled" : "Disabled");
        MessageOutput.printf("%s\r\n", getText(Text::T_HEAD).data());

        // "Open circuit voltage"
        printOpenCircuitVoltageReport();

        // "Low voltage power limiter"
    }

    // "Periodically recharge the battery"
    if (_useRechargeTheBattery) {

    }

    if (_verboseReport) {
        //MessageOutput.printf("%s\r\n", getText(Text::T_HEAD).data());
        MessageOutput.printf("%s --------------------------------------------------------------------------------\r\n",
            getText(Text::T_HEAD).data());
        MessageOutput.printf("%s\r\n", getText(Text::T_HEAD).data());
    }
}


/*
 * Update the battery guard with new values. (voltage[V], current[A], millisStamp[ms])
 * Note: Just call the function if new values are available. Current into the battery must be positive.
 */
void BatteryGuardClass::updateBatteryValues(float const voltage, float const current, uint32_t const millisStamp) {
    if (_useBatteryGuard && (voltage >= 0.0f)) {

        // analyse the measurement period, this information is used by the "low voltage limit" feature
        if ((_battMillis != 0) && (voltage != _battVoltage)) {
            _determinedPeriod.addNumber(millisStamp - _battMillis);
        }

        // analyse the voltage and current resolution
        // SW-Nico: Actual I do not trust the battery stats information (for smart shunt they are wrong)
        auto resolution = std::abs(voltage - _battVoltage);
        if ((resolution >= 0.001f) && (resolution < _determinedResolutionV)) { _determinedResolutionV = resolution; }
        resolution = std::abs(current - _battCurrent);
        if ((resolution >= 0.001f) && (resolution < _determinedResolutionI)) { _determinedResolutionI = resolution; }

        _battVoltage = voltage;
        _battCurrent = current;
        _battMillis = millisStamp;
        _battVoltageAVG.addNumber(_battVoltage);
        calculateInternalResistance(_battVoltage, _battCurrent);
        calculateOpenCircuitVoltage(_battVoltage, _battCurrent);
    }
}


/*
 * Calculate the battery open circuit voltage.
 * Returns true if a new value was calculated
 */
bool BatteryGuardClass::calculateOpenCircuitVoltage(float const nowVoltage, float const nowCurrent) {

    // calculate the open circuit battery voltage (current flow into the battery must be positive)
    auto oResistor = getInternalResistance();
    if (oResistor.has_value()) {
        _openCircuitVoltageAVG.addNumber(nowVoltage - nowCurrent * oResistor.value());
        return true;
    }
    return false;
}


/*
 * Returns the battery internal resistance, calculated / configured or nullopt if neither value is valid
 */
std::optional<float> BatteryGuardClass::getInternalResistance(void) const {
    // we use the calculated value if we have enough data (5 calculations minimum)
    if (_resistanceFromCalcAVG.getCounts() > 4) { return _resistanceFromCalcAVG.getAverage(); }
    if (_resistanceFromConfig != 0.0f) { return _resistanceFromConfig; }
    return std::nullopt;
}


/*
 * Returns the battery open circuit voltage or nullopt if value is not valid
 */
std::optional<float> BatteryGuardClass::getOpenCircuitVoltage(void) {
  if ((_openCircuitVoltageAVG.getCounts() > 0) && isDataValid()) {
        return _openCircuitVoltageAVG.getAverage();
    } else {

        #ifdef DEBUG_LOGGING
        _notAvailableCounter++;
        #endif

        return std::nullopt;
    }
}


/*
 * true if the measurement resolution and measurement period is sufficient
 */
bool BatteryGuardClass::isResolutionOK(void) const {
    return (_determinedResolutionV <= 0.020f) && (_determinedResolutionI <= 0.100f)
        && (_determinedPeriod.getAverage() <= 4000);
}


/*
 * Calculate the battery resistance between the battery cells and the voltage measurement device.
 * Returns true if a new resistance value was calculated
 * Note: The calculation will not work if the power change is too low or too slow.
 */
bool BatteryGuardClass::calculateInternalResistance(float const nowVoltage, float const nowCurrent) {

    bool result = false;
    auto minDiffCurrent = 3.5f; // seems to be a good value for all batteries

    // check the resolution of the voltage and current measurement
    if (!isResolutionOK()) { return result; }

    // check for trigger (sufficient current change)
    if (!_triggerEvent && _minMaxAvailable && (std::abs(nowCurrent - _pMinVolt.second) > (minDiffCurrent/3.0f))) {
        _lastTriggerMillis = millis();
        _triggerEvent = true;
    }

    // we evaluate min and max values in a time duration of 15 sec after the trigger event
    if (!_triggerEvent || (_triggerEvent && (millis() - _lastTriggerMillis) < 15*1000)) {

        // we must avoid to use measurement values during any power transition or if voltage and
        // current measurement have different time stamps
        // To solve this problem, we check whether two consecutive measurements are almost identical
        if (!_firstOfTwoAvailable || (std::abs(_pFirstVolt.first - nowVoltage) > 0.005f) ||
            (std::abs(_pFirstVolt.second - nowCurrent) > 0.2f)) {
            _pFirstVolt.first = nowVoltage;
            _pFirstVolt.second = nowCurrent;
            _firstOfTwoAvailable = true;
            return result;
        }
        _firstOfTwoAvailable = false;  // preparation for the next two consecutive values

        auto avgVolt = std::make_pair((nowVoltage+_pFirstVolt.first)/2.0f, (nowCurrent+_pFirstVolt.second)/2.0f);
        if (!_minMaxAvailable || !_triggerEvent) {
            _pMinVolt = _pMaxVolt = avgVolt;
            _minMaxAvailable = true;
        } else {
            if (avgVolt.first < _pMinVolt.first) { _pMinVolt = avgVolt; }
            if (avgVolt.first > _pMaxVolt.first) { _pMaxVolt = avgVolt; }
        }
        return result;
    }
    // reset conditions for the next calculation
    _firstOfTwoAvailable = false;
    _minMaxAvailable = false;
    _triggerEvent = false;

    // now we have the minimum and maximum values, we can try to calculate the resistance
    // we need a minimum difference to get a sufficiently good result (failure < 10%)
    // SmartShunt: 40mV and 3.5A (about 100W on VDC: 24V, Ri: 12mOhm)
    auto minDiffVoltage = (_determinedResolutionV > 0.005f) ? 0.07f : 0.04f;
    auto diffVolt = _pMaxVolt.first - _pMinVolt.first;
    auto diffCurrent = std::abs(_pMaxVolt.second - _pMinVolt.second);   // can be negative
    if ((diffVolt >= minDiffVoltage) && (diffCurrent >= minDiffCurrent)) {
        float resistor = diffVolt / diffCurrent;

        // safety feature: we try to keep out bad values from the average
        auto reference = getInternalResistance();
        if (reference.has_value()) {
            if ((resistor > reference.value() / 2.0f) && (resistor < reference.value() * 2.0f)) {
                _resistanceFromCalcAVG.addNumber(resistor);
                result = true;
            }
        } else {
            _resistanceFromCalcAVG.addNumber(resistor);
            result = true;
        }
    }
    return result;
}


/*
 * Prints the "Battery open circuit voltage" report
 */
void BatteryGuardClass::printOpenCircuitVoltageReport(void)
{
    //MessageOutput.printf("%s\r\n", getText(Text::T_HEAD).data());
    MessageOutput.printf("%s 1) Battery open circuit voltage: %s / Battery data %s\r\n",
        getText(Text::T_HEAD).data(), (_useBatteryGuard) ? "Enabled" : "Disabled",
        (isResolutionOK()) ? "sufficient" : "not sufficient");

    MessageOutput.printf("%s Open circuit voltage: %0.3fV (Actual voltage: %0.3fV, Avarage voltage: %0.3fV)\r\n",
        getText(Text::T_HEAD).data(), _openCircuitVoltageAVG.getAverage(), _battVoltage, _battVoltageAVG.getAverage());

  auto oResistance = getInternalResistance();
    if (!oResistance.has_value()) {
        MessageOutput.printf("%s Resistance neither calculated (5 times) nor configured\r\n", getText(Text::T_HEAD).data());
    } else {
        auto resCalc = (_resistanceFromCalcAVG.getCounts() > 4) ? _resistanceFromCalcAVG.getAverage() * 1000.0f : 0.0f;
        MessageOutput.printf("%s Resistance in use: %0.1fmOhm (Calculated: %0.1fmOhm, Configured: %0.1fmOhm)\r\n",
            getText(Text::T_HEAD).data(), oResistance.value() * 1000.0f, resCalc, _resistanceFromConfig * 1000.0f);
    }

    #ifdef DEBUG_LOGGING
    MessageOutput.printf("%s Calculated resistance: %0.1fmOhm (Min: %0.1f, Max: %0.1f, Last: %0.1f, Amount: %i)\r\n",
        getText(Text::T_HEAD).data(), _resistanceFromCalcAVG.getAverage()*1000.0f, _resistanceFromCalcAVG.getMin()*1000.0f,
        _resistanceFromCalcAVG.getMax()*1000.0f, _resistanceFromCalcAVG.getLast()*1000.0f, _resistanceFromCalcAVG.getCounts());

        MessageOutput.printf("%s Voltage resolution: %0.0fmV, Current resolution: %0.0fmA, Period: %ims\r\n",
            getText(Text::T_HEAD).data(), _determinedResolutionV * 1000.0f, _determinedResolutionI * 1000.0f,
            _determinedPeriod.getAverage());

        MessageOutput.printf("%s Open circuit voltage not available counter: %i\r\n",
            getText(Text::T_HEAD).data(),  _notAvailableCounter);

        MessageOutput.printf("%s Battery voltage statistics: %0.3fV (Min: %0.3f, Max: %0.3f, Last: %0.3f, Amount: %i)\r\n",
            getText(Text::T_HEAD).data(), _battVoltageAVG.getAverage(), _battVoltageAVG.getMin(),
            _battVoltageAVG.getMax(), _battVoltageAVG.getLast(), _battVoltageAVG.getCounts());
    #endif
}


/*
 * Returns a string according to current text number
 */
frozen::string const& BatteryGuardClass::getText(BatteryGuardClass::Text tNr)
{
    static const frozen::string missing = "programmer error: missing status text";

    static const frozen::map<Text, frozen::string, 5> texts = {
        { Text::Q_NODATA, "Insufficient data" },
        { Text::Q_EXCELLENT, "Excellent" },
        { Text::Q_GOOD, "Good" },
        { Text::Q_BAD, "Bad" },
        { Text::T_HEAD, "[BatteryGuard]"}
    };

    auto iter = texts.find(tNr);
    if (iter == texts.end()) { return missing; }

    return iter->second;
}
