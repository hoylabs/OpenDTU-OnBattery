// SPDX-License-Identifier: GPL-2.0-or-later

/* Surplus-Power-Mode
 *
 * The Surplus-Power-Mode regulates the inverter output power based on the surplus solar power.
 * Surplus solar power is available when the battery is almost full and the available
 * solar power is higher than the power consumed in the household.
 * The secondary intention is not to waste energy, if it is not possible to fully charge the battery
 * by the end of the day.
 *
 * Basic principle of Surplus-Stage-I (MPPT in bulk mode):
 * In bulk mode the MPPT acts like a current source.
 * In this mode we get reliable maximum solar power information from the MPPT and can use it for regulation.
 * We do not use all solar power for the inverter. We must reserve power for the battery to reach fully charge
 * of the battery until end of the day.
 * The calculation of the "reserve power" is based on actual SoC and remaining time to sunset.
 *
 * Basic principle of Surplus-Stage-II (MPPT in absorption/float mode):
 * In absorption- and float-mode the MPPT acts like a voltage source with current limiter.
 * In these modes we don't get reliable information about the maximum solar power or current from the MPPT.
 * To find the maximum solar power we increase the inverter power, we go higher and higher, step by step,
 * until we reach the maximum solar power. On this point the MPPT current limiter will kick in and the voltage
 * begins to drop. When we go one step back and check if the voltage is back above the target voltage.
 * A kind of simple approximation control.
 *
 * Basic principle of regulation quality indication (Excellent - Good - Bad)
 * To give an hint, if regulation can handle your system, we included regulation quality indication
 * We count every power step polarity change ( + to -  and - to +) until we reach the state "IN_TARGET".
 * Normally only one polarity change is necessary to reach the target.
 * If we need sometimes more .. no problem, but if we are permanent above 2 we have a problem and can
 * not regulate the surplus power on this particular system.
 *
 * Notes:
 * We need Victron VE.Direct Rx/Tx (text-mode and hex-mode) to get the MPPT absorption- and
 * float-voltage
 *
 * 10.08.2024 - 1.00 - first version, Stage-II (absorption-/float-mode)
 * 30.11.2024 - 1.10 - add of Stage-I (bulk-mode) and minor improvements of Stage-II
 * 04.02.2025 - 1.20 - use solar charger class, add of report and minor improvements
 * 25.02.2025 - 1.30 - new feature: optional slope mode in Stage-I
 *                     improvement: use battery and inverter to calculate the total solar panel power
 */

#include <solarcharger/Controller.h>
#include <battery/Controller.h>
#include <frozen/map.h>
#include "Configuration.h"
#include "MessageOutput.h"
#include "SunPosition.h"
#include "SurplusPower.h"

// activate the following line to enable debug logging
//#define DEBUG_LOGGING


using CHARGER_STATE = SolarChargers::Stats::StateOfOperation;
constexpr int RESERVE_POWER_MAX = 99999;        // Default value, battery reserve power [W]
constexpr float EFFICIENCY_MPPT = 0.97f;        // 97%, constant value is good enough for the surplus calculation
constexpr float EFFICIENCY_INVERTER = 0.94f;    // 94%, constant value is good enough for the surplus calculation
constexpr float EFFICIENCY_CABLE = 0.98f;       // 98%, means 2% power loss on the cable
constexpr float ABSORPTION_SOC = 0.998f;        // 99.8%, seems to be a good value for all charger
constexpr float TARGET_RANGE = 0.100f;          // 100mV, target voltage range [V]
constexpr float SOC_RANGE = 2.0f;               // 2% SoC, stop range for stage-I [SoC]

SurplusPowerClass SurplusPower;


/*
 * Initialize the battery guard
 */
void SurplusPowerClass::init(Scheduler& scheduler) {

    // init the task loop
    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&SurplusPowerClass::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.setInterval(60*1000);
    _loopTask.enable();

    updateSettings();
}


/*
 * Periodical tasks, will be called once a minute
 */
void SurplusPowerClass::loop(void) {

    if (!isSurplusEnabled() ) { return; }

    if (_verboseReport) { printReport(); }
}


/*
 * Updating the class parameters for calculating the surplus power.
 */
void SurplusPowerClass::updateSettings(void) {

    //_verboseLogging = config.PowerLimiter.VerboseLogging;
    _verboseLogging = false;            // surplus verbose logging
    _verboseReport = true;              // surplus verbose report

    // todo: get the parameter for stage-I from the configuration
    // necessary parameter to use stage-I
    _stageIEnabled = false;             // surplus-stage-I (bulk-mode) enable / disable
    _startSoC = 50.0f;                  // [%] stage-I, start SoC
    _batteryCapacity = 5000;            // [Wh] stage-I, battery capacity, todo: get the real value from the battery?
    _batterySafetyPercent = 50.0f;      // [%] stage-I, battery reserve safety factor, 30% more es calculated
    _durationAbsorptionToSunset = 60;   // [Minutes] stage-I, duration from absorption to sunset
    _surplusIUpperPowerLimit = 0;       // [W] upper power limit, if 0 we use the DPL total upper power limit

    // todo: get the parameter for stage-I, option slope-mode from the configuration
    // necessary parameter to use the slope mode
    _slopeModeEnabled = false;          // surplus-stage-I (bulk-mode) enable / disable
    _slopeAddPower = 25;                // [W] added to the real consumption
    _slopeFactor = 3;                   // [W/s] slope power reduction by 3W pro 1 sec

    // todo: get the parameter for stage-II from the configuration
    // necessary parameter to use stage-II
    _stageIIEnabled = true;             // surplus-stage-II (absorption-mode) enable / disable
    _surplusIIUpperPowerLimit = 0;      // [W] upper power limit, if 0 we use the DPL total upper power limit

    // make sure to be inside lower and upper bounds
    // todo: better move to web UI?
    if ((_startSoC < 40.0f) || (_startSoC > 100.0f)) { _startSoC = 70.0f; }
    if ((_batteryCapacity < 100) || (_batteryCapacity > 40000)) { _batteryCapacity = 2500; }
    if ((_batterySafetyPercent < 0.0f) || (_batterySafetyPercent > 100.0f)) { _batterySafetyPercent = 50.0f; }
    if ((_durationAbsorptionToSunset < 0) || (_durationAbsorptionToSunset > 4*60)) { _durationAbsorptionToSunset = 60; }

    // todo: maybe better to use sum of all battery powered inverter as DPL upper power limit?
    auto const& config = Configuration.get();
    if (_surplusIUpperPowerLimit == 0) { _surplusIUpperPowerLimit = config.PowerLimiter.TotalUpperPowerLimit; }
    if (_surplusIIUpperPowerLimit == 0) { _surplusIIUpperPowerLimit = config.PowerLimiter.TotalUpperPowerLimit; }

    // power steps for the approximation regulation of stage-II (50W Steps on inverter with 2000W)
    // the power step size should not be below the hysteresis, otherwise one step has no effect
    _powerStepSize = (_surplusIIUpperPowerLimit) / 40;
    _powerStepSize = std::max(_powerStepSize, static_cast<int16_t>(config.PowerLimiter.TargetPowerConsumptionHysteresis)) + 1;
    _batteryReserve = RESERVE_POWER_MAX;
}


/*
 * Returns the "surplus power" or the "requested power", whichever is higher
 * requestedPower:  The power based on actual calculation from "Zero feed throttle"
 * nowPower:        The actual inverter power
 * nowMillis:       Millis() of the last inverter limit update
 */
uint16_t SurplusPowerClass::calculateSurplus(uint16_t const requestedPower, uint16_t const nowPower, uint32_t const nowMillis) {

    if (!isSurplusEnabled()) { return requestedPower; }

    auto oState = SolarCharger.getStats()->getStateOfOperation();
    if (!oState.has_value()) {
        return exitSurplus(requestedPower, requestedPower, ExitState::ERR_CHARGER); // no information, we aboard
    }

    if (_stageIEnabled && !_stageITempOff && (oState.value() == CHARGER_STATE::Bulk)) {
        return calcBulkMode(requestedPower, nowPower, nowMillis); // return from Stage-I
    }

    if (_stageIIEnabled && !_stageIITempOff &&
        ((oState.value() == CHARGER_STATE::Absorption) || (oState.value() == CHARGER_STATE::Float))) {
        return calcAbsorptionFloatMode(requestedPower, oState.value(), nowMillis); // return from Stage-II
    }

    triggerStageState(false, false); // for the report
    return requestedPower; // solar charger in wrong operation state, we stop surplus
}


/*
 * Calculates the surplus-power-stage-II (absorption-/float-mode)
 * requestedPower:  The power based on actual calculation from "Zero feed throttle"
 * modeAF:          Absorption or float mode
 * nowMillis:       Millis() of the last inverter limit update
 */
uint16_t SurplusPowerClass::calcAbsorptionFloatMode(uint16_t const requestedPower, SolarChargers::Stats::StateOfOperation const modeAF,
                                                    uint32_t const nowMillis) {

    // the regulation loop "Inverter -> Solar Charger -> Solar Panel -> Measurement" needs time. And different to
    // "Zero-Feed-Throttle" the "Surplus-Mode" is not in a hurry. We always wait 5 sec before
    // we do the next calculation. In the meantime we use the value from the last calculation
    if ((_stageIIActive) && (((millis() - _lastCalcMillis) < 5000) || ((millis() - nowMillis) < 2000))) {
        return exitSurplus(requestedPower ,_surplusPower, ExitState::OK_STAGE_II);
    }

    // get the absorption and float voltage from the solar charger
    auto const& oAbsorptionVoltage = SolarCharger.getStats()->getAbsorptionVoltage();
    auto const& oFloatVoltage = SolarCharger.getStats()->getFloatVoltage();
    if ((!oAbsorptionVoltage.has_value()) || (!oFloatVoltage.has_value())) {
        return exitSurplus(requestedPower, requestedPower, ExitState::ERR_CHARGER); // we aboard
    }

    // set the regulation target voltage threshold
    // Note: This may be the tricky part of the regulation. Can be, we have to calculate the "magic number" from system parameter
    // like charger power, inverter power and battery voltage? If I calculate with internal solar charger resistance
    // of max 2mOhm and max 50A. The voltage will drop by 100mV. So .. 100mV is a good value
    _targetVoltage = (modeAF == CHARGER_STATE::Absorption) ? oAbsorptionVoltage.value() : oFloatVoltage.value();
    _targetVoltage -= TARGET_RANGE;

    // get the actual battery voltage from the solar charger
    // Note: We use the identical voltage like the solar charger
    auto const& oMpptVoltage = SolarCharger.getStats()->getOutputVoltage();
    if (!oMpptVoltage.has_value()) {
        return exitSurplus(requestedPower, requestedPower, ExitState::ERR_CHARGER); // we aboard
    }
    auto mpptVoltage = oMpptVoltage.value();
    _avgMPPTVoltage.addNumber(mpptVoltage);
    auto avgMPPTVoltage = _avgMPPTVoltage.getAverage();

    // state machine: hold, increase or decrease the surplus power
    int16_t addPower = 0;
    switch (_surplusState) {

        case State::IDLE:
        case State::BULK_POWER:
            // if stage-I was active before, we can start stage-II maybe with the identical power
            _surplusPower = std::max(_surplusPower, static_cast<int32_t>(requestedPower));
            _surplusState = State::TRY_MORE;
            _slopePower = 0;
            _qualityCounter = 0;
            _qualityAVG.reset();
            triggerStageState(false, true); // for the report
            break;

        case State::KEEP_LAST_POWER:
            // during last regulation step the requested power was higher as the surplus power
            if (mpptVoltage >= _targetVoltage) {
                // again above the target voltage, we try to increase the power
                _surplusState = State::TRY_MORE;
                addPower = _powerStepSize;
            } else {
                // below the target voltage, we keep the last surplus power but change the state
                _surplusState = State::REDUCE_POWER;
            }
            break;

        case State::TRY_MORE:
            if (mpptVoltage >= _targetVoltage) {
                // still above the target voltage, we increase the power
                addPower = 2*_powerStepSize;
            } else {
                // below the target voltage, we need less power
                addPower = -_powerStepSize;     // less power
                _surplusState = State::REDUCE_POWER;
            }
            break;

        case State::REDUCE_POWER:
            if (mpptVoltage >= _targetVoltage) {
                // we hit the target after reducing the surplus power
                // now we use maximum solar power
                _lastInTargetMillis = millis();
                _surplusState = State::IN_TARGET;
            } else {
                // still below the target voltage, we need less power
                addPower = -_powerStepSize;
            }
            break;

        case State::MAXIMUM_POWER:
        case State::IN_TARGET:
            // here we use both ... the actual and the average voltage
            if ((avgMPPTVoltage >= _targetVoltage) || (mpptVoltage >= _targetVoltage)) {
                // we are in the target rage but ... maybe more power is possible?
                // we try to increase the power after a time out of 1 minute
                if ((millis() - _lastInTargetMillis) > 60 * 1000) {
                    addPower = _powerStepSize;   // lets try if more power is possible
                    _surplusState = State::TRY_MORE;
                }
                // regulation quality: we reached the target
                if (_qualityCounter != 0) { _qualityAVG.addNumber(_qualityCounter); }
                _qualityCounter = 0;
            } else {
                // out of the target voltage we must reduce the power
                addPower = -_powerStepSize;
                _surplusState = State::REDUCE_POWER;
            }
            break;

        default:
                addPower = 0;
                _surplusState = State::IDLE;

    }
    _surplusPower += addPower;

    // we do not go below 0W or above the upper power limit
    _surplusPower = std::max(_surplusPower, 0);
    if (_surplusPower > _surplusIIUpperPowerLimit) {
        _surplusPower = _surplusIIUpperPowerLimit;
        _surplusState = State::MAXIMUM_POWER;
    }

    // we do not go below the requested power
    auto backPower = static_cast<uint16_t>(_surplusPower);
    if (requestedPower > backPower) {
        _qualityCounter = 0;
        _surplusState = State::KEEP_LAST_POWER;
    } else {
        // regulation quality: count the polarity changes
        if (((_lastAddPower < 0) && (addPower > 0)) || ((_lastAddPower > 0) && (addPower < 0))) {
            _qualityCounter++;
        }
        _lastAddPower = addPower;
    }

    _lastCalcMillis = millis();
    return exitSurplus(requestedPower, backPower, ExitState::OK_STAGE_II);
}


/*
 * Calculates the surplus-power-stage-I (bulk-mode)
 * requestedPower:  The power based on actual calculation of "Zero feed throttle" or "Solar passthrough"
 * nowPower:        The actual inverter power
 * nowMillis:       The last inverter update
 */
uint16_t SurplusPowerClass::calcBulkMode(uint16_t const requestedPower, uint16_t const nowPower, uint32_t const nowMillis) {

    // Different to "Zero-Feed-Throttle" the "Surplus-Mode" is not in a hurry. We always wait 10 sec before
    // we do the next calculation or 2 sec after any inverter power change.
    // In the meantime we use the values from the last calculation
    // Note: The slope mode keeps the 1 sec calculation time
    if ((_stageIActive) && (((millis() - _lastCalcMillis) < 10000) || ((millis() - nowMillis) < 2000))) {
        auto backPower = _surplusPower;
        if (_slopeModeEnabled) { backPower = calcSlopePower(requestedPower, _surplusPower); }
        return exitSurplus(requestedPower ,backPower, ExitState::OK_STAGE_I);
    }

    // prepared for future: these values can also get "start voltage" instead of "start SoC"
    // Note: We also need the SoC calculated from the battery voltage before we can offer this
    float startValue = _startSoC;
    float stopValue  = _startSoC - SOC_RANGE;
    float actValue = 0.0f;
    int32_t actBatteryPower = 0;

    // get the actual SoC from the battery provider
    if (!Battery.getStats()->isSoCValid() || Battery.getStats()->getSoCAgeSeconds() > 10) {
        return exitSurplus(requestedPower, requestedPower, ExitState::ERR_BATTERY);
    }
    actValue = Battery.getStats()->getSoC();
    actBatteryPower = static_cast<int32_t>(Battery.getStats()->getChargeCurrent() * Battery.getStats()->getVoltage());

    // below the stop threshold or below start threshold or no time left?
    if ((actValue <= stopValue) || ((actValue < startValue) && !_stageIActive) || ((getTimeToSunset() - _durationAbsorptionToSunset) < 0)) {
        triggerStageState(false, false);
        return exitSurplus(requestedPower, requestedPower, ExitState::OK_STAGE_I); // normal stop from Stage-I
    }

    if (_surplusState != State::BULK_POWER) {
        _batteryReserve = RESERVE_POWER_MAX;
        _lastReserveCalcMillis = 0;
        _surplusPower = _slopePower = 0;
        _solarCounter = {0, 0};
        triggerStageState(true, false);
        _surplusState = State::BULK_POWER;
    }

    // calculate the solar power from "the inverter power and the battery power" or from "the solar charger(s)"
    uint16_t solarBattPower = 0;
    uint16_t solarPanelPower = 0;
    if (Battery.getStats()->isCurrentValid()) {
        solarBattPower = nowPower / EFFICIENCY_INVERTER / EFFICIENCY_CABLE + actBatteryPower;
    }
    if (SolarCharger.getStats()->getPanelPowerWatts().has_value()) {
        solarPanelPower = SolarCharger.getStats()->getPanelPowerWatts().value() * EFFICIENCY_MPPT;
    }

    if ((solarBattPower == 0) && (solarPanelPower == 0)) {
        return exitSurplus(requestedPower, requestedPower, ExitState::ERR_SOLAR_POWER);
    }
    if (solarBattPower > solarPanelPower) {
        _solarPower = solarBattPower; // Battery power + Inverter power
        _solarCounter.first++;
    } else {
        _solarPower = solarPanelPower; // Solar charger panel power
        _solarCounter.second++;
    }

    // calculating the battery reserve power after start and later in a fixed period of 5 minutes
    // It is not necessary to do this more often because the battery reserve power does not change that quickly
    if ((_lastReserveCalcMillis == 0) || ((millis() - _lastReserveCalcMillis) > 5*60*1000)) {

        _lastReserveCalcMillis = millis();
        auto actSoC = actValue; // prepared for future: can also be derived from battery voltage

        // time from now to start of absorption mode in minutes
        _durationNowToAbsorption = getTimeToSunset() - _durationAbsorptionToSunset;

        // calculation of the power, we want to reserve for the battery
        if (_durationNowToAbsorption > 0) {
            // in the last 10 minutes we keep the reserve power constant
            if ((_durationNowToAbsorption > 10) && (actSoC / 100.0f < ABSORPTION_SOC)) {
                // enough time left, so we can calculate the battery reserve power
                _batteryReserve = _batteryCapacity * (ABSORPTION_SOC - actSoC / 100.0f) / _durationNowToAbsorption * 60 *
                    (1.0f + _batterySafetyPercent / 100.0f);
                _batteryReserve = std::max(_batteryReserve, 0); // avoid negative values
            }
        } else {
            // time is over, but we did not reach the absorption mode, so we use maximum reserve power
            _batteryReserve = RESERVE_POWER_MAX;
            _durationNowToAbsorption = 0;  // avoid negative values
        }
    }

    // surplus power, including inverter power loss
    _surplusPower = (_solarPower - _batteryReserve) * EFFICIENCY_INVERTER;
    _surplusPower = std::max(_surplusPower, 0); // avoid negative values
    _surplusPower = std::min(_surplusPower, static_cast<int32_t>(_surplusIUpperPowerLimit));

    // slope power
    auto backPower = static_cast<uint16_t>(_surplusPower);
    if (_slopeModeEnabled) { backPower = calcSlopePower(requestedPower, _surplusPower); }

    _lastCalcMillis = millis();
    return exitSurplus(requestedPower, backPower, ExitState::OK_STAGE_I);
}


/*
 * Calculates the slope power
 * requestedPower:  The actual "Zero feed throttle power" [W]
 * surplusPower:    The actual "surplus power" [W]
 * return:          The "slope power" [W]
 */
uint16_t SurplusPowerClass::calcSlopePower(uint16_t const requestedPower, int32_t const surplusPower) {

     // calculate the decrease of the slope mode, not faster as once in a second
     if (millis() - _lastSlopeMillis > 1000) {

        _slopePower -= _slopeFactor * (millis() - _lastSlopeMillis) / 1000;
        if (_slopePower < 0) { _slopePower = 0; } // avoid negative values

        #ifdef DEBUG_LOGGING
        auto factor = _slopeFactor * (millis() - _lastSlopeMillis) / 1000;
        MessageOutput.printf("%s Slope: %iW, Factor: %0.4f, Millis: %lums\r\n",
            getText(Text::T_HEAD1).data(), _slopePower, factor, millis() - _lastSlopeMillis);
        #endif

        _lastSlopeMillis = millis();
    }

    // check the minimum distance between requested power and slope power
    auto consumption = requestedPower - Configuration.get().PowerLimiter.TargetPowerConsumption;
    if (consumption < 0) { consumption = 0; } // avoid negative values
    if (_slopePower < (consumption + _slopeAddPower)) { _slopePower = consumption + _slopeAddPower; }
    _slopePower = std::min(_slopePower, surplusPower);
    return static_cast<uint16_t>(_slopePower);
}


/*
 * Print the logging information if logging is enabled and surplus power has changed
 */
uint16_t SurplusPowerClass::exitSurplus(uint16_t const requestedPower, uint16_t const exitPower, SurplusPowerClass::ExitState const exitState) {

    if ((exitState != SurplusPowerClass::ExitState::OK_STAGE_I) && (exitState != SurplusPowerClass::ExitState::OK_STAGE_II)) {
        auto fault = static_cast<size_t>(exitState);
        if (fault < _errorCounter.size()) { _errorCounter[fault]++; }
        MessageOutput.printf("%s %s\r\n", getText(Text::T_HEAD).data(), getExitText(exitState).data());
        return requestedPower; // return the requested power on any detected fault
    }

    if (exitPower > requestedPower) {
        if (exitPower == _lastLoggingPower) { return exitPower; } // to avoid logging of useless information
        _lastLoggingPower = exitPower;

        if (_verboseLogging && (exitState == SurplusPowerClass::ExitState::OK_STAGE_I)) {
            MessageOutput.printf("%s State: %s, Surplus power: %iW, Slope power: %iW, Requested power: %iW, Returned power: %iW\r\n",
                getText(Text::T_HEAD1).data(), getStatusText(_surplusState).data(),
                _surplusPower, _slopePower, requestedPower, exitPower);
        }
        if (_verboseLogging && (exitState == SurplusPowerClass::ExitState::OK_STAGE_II)) {
            MessageOutput.printf("%s State: %s, Surplus power: %iW, Requested power: %iW, Returned power: %iW\r\n",
                getText(Text::T_HEAD2).data(), getStatusText(_surplusState).data(),
                _surplusPower, requestedPower, exitPower);
        }
        return exitPower;
    } else {
        return requestedPower; // No logging, if surplus power is below the requested power
    }
}


/*
 * Returns time to sunset im minutes if actual time is between 0:00 - sunset
 * or 0 if actual time is between sunset - 24:00
 */
int16_t SurplusPowerClass::getTimeToSunset(void) {

    // save processing time, if the function is called more than once in a minute
    if ((_lastTimeMillis != 0) && (millis() - _lastTimeMillis < 60 * 1000)) { return _timeToSunset; }

    _lastTimeMillis = millis();
    struct tm actTime, sunsetTime;
    if (getLocalTime(&actTime, 10) && SunPosition.sunsetTime(&sunsetTime)) {
        _timeToSunset = sunsetTime.tm_hour * 60 + sunsetTime.tm_min - actTime.tm_hour * 60 - actTime.tm_min;
        if (_timeToSunset < 0) { _timeToSunset = 0; }
    } else {
        _errorCounter[static_cast<uint8_t>(ExitState::ERR_TIME)]++;
    }
    return _timeToSunset;
}


/*
 * Temporary switch-off the "surplus power" for stage-I or stage-II.
 * For example: If a battery manager is on the way to force a full charge of the battery
 * or if a AC-Charger is active.
 */
bool SurplusPowerClass::switchSurplusOnOff(SurplusPowerClass::Switch const onoff) {
    bool answer = true;

    switch (onoff) {

        case Switch::STAGE_I_ON:
            _stageITempOff = false;
            break;

        case Switch::STAGE_II_ON:
            _stageIITempOff = false;
            break;

        case Switch::STAGE_I_OFF:
            _stageITempOff = true;
            _surplusPower = 0;
            _surplusState = State::IDLE;
            answer = false;
            break;

        case Switch::STAGE_II_OFF:
            _stageIITempOff = true;
            _surplusPower = 0;
            _surplusState = State::IDLE;
            answer = false;
            break;

        case Switch::STAGE_I_ASK:
            answer = !_stageITempOff;
            break;

        case Switch::STAGE_II_ASK:
            answer = !_stageIITempOff;
            break;
    }
    return answer;
}


/*
 * Returns true if "surplus power" stage-I or stage-II is enabled
 */
bool SurplusPowerClass::isSurplusEnabled(void) const {
    return (_stageIEnabled || _stageIIEnabled);
}


void SurplusPowerClass::triggerStageState(bool stageI, bool stageII) {

    if (stageI && !_stageIActive) {
        _stageIActive = true;
        getLocalTime(&_stage_I_Time_Start, 10);
        _oUpperPowerLimit = _surplusIUpperPowerLimit;
    }
    if (!stageI && _stageIActive) {
        _stageIActive = false;
        getLocalTime(&_stage_I_Time_Stop, 10);
    }
    if (stageII && !_stageIIActive) {
        _stageIIActive = true;
        getLocalTime(&_stage_II_Time_Start, 10);
        _oUpperPowerLimit = _surplusIIUpperPowerLimit;
    }
    if (!stageII && _stageIIActive) {
        _stageIIActive = false;
        getLocalTime(&_stage_II_Time_Stop, 10);
    }
    if (!_stageIActive && !_stageIIActive) {
        _surplusState = State::IDLE;
        _surplusPower = _slopePower = 0;
        _oUpperPowerLimit = std::nullopt;
    }
}

/*
 * Prints the report of the "surplus power" mode
 * todo: maybe better to use the web UI for the report?
 */
void SurplusPowerClass::printReport(void)
{
    MessageOutput.printf("%s\r\n", getText(Text::T_HEAD).data());
    MessageOutput.printf("%s ---------------- Surplus Report (every minute) ----------------\r\n",
        getText(Text::T_HEAD).data());

    MessageOutput.printf("%s Surplus: %s\r\n", getText(Text::T_HEAD).data(),
        isSurplusEnabled() ? "Enabled" : "Disabled");

    MessageOutput.printf("%s State: %s, Surplus power: %iW (max: %iW)\r\n",
        getText(Text::T_HEAD).data(), getStatusText(_surplusState).data(), _surplusPower,
        (_stageIIActive) ? _surplusIIUpperPowerLimit : _surplusIUpperPowerLimit);

    auto oState = SolarCharger.getStats()->getStateOfOperation();
    if (oState.has_value()) {
        auto vStOfOp = oState.value();
        MessageOutput.printf("%s Solar charger operation mode: %s\r\n",
            getText(Text::T_HEAD).data(), (vStOfOp==CHARGER_STATE::Bulk) ? "Bulk" : (vStOfOp==CHARGER_STATE::Absorption ? "Absorption" :
            (vStOfOp==CHARGER_STATE::Float) ? "Float" : "Off"));
    }
    MessageOutput.printf("%s Errors since start-up, Time: %i, Solar charger: %i, Battery: %i\r\n",
        getText(Text::T_HEAD).data(), _errorCounter[static_cast<uint8_t>(ExitState::ERR_TIME)],
        _errorCounter[static_cast<uint8_t>(ExitState::ERR_CHARGER)], _errorCounter[static_cast<uint8_t>(ExitState::ERR_BATTERY)]);


    MessageOutput.printf("%s\r\n", getText(Text::T_HEAD).data());
    MessageOutput.printf("%s 1) Stage-I (Bulk): %s / %s\r\n",
        getText(Text::T_HEAD).data(), (_stageIEnabled && !_stageITempOff) ? "Enabled" : "Disabled",
        (_surplusState == State::BULK_POWER) ? "Active" : "Not active");

    MessageOutput.printf("%s SoC Start: %0.1f%%, SoC Stop: %0.1f%%, Actual SoC: %0.1f%%\r\n",
        getText(Text::T_HEAD).data(), _startSoC, _startSoC - SOC_RANGE, Battery.getStats()->getSoC());

    MessageOutput.printf("%s Slope Mode: %s, Slope Power: %iW (max: %iW)\r\n", getText(Text::T_HEAD).data(),
        (_slopeModeEnabled) ? "Enabled" : "Disabled", _slopePower, _surplusPower);

    if (_surplusState == State::BULK_POWER) {
        MessageOutput.printf("%s Solar power: %iW, Battery reserved power: %iW\r\n",
                getText(Text::T_HEAD).data(), _solarPower, _batteryReserve);

        MessageOutput.printf("%s Time remaining from now to absorption mode: %02i:%02i\r\n",
                getText(Text::T_HEAD).data(), _durationNowToAbsorption / 60, _durationNowToAbsorption % 60);

        float solar = 0.0f;
        float battery = 0.0f;
        if (_solarCounter.second + _solarCounter.first > 0.0f) {
            solar = _solarCounter.second / (_solarCounter.second + _solarCounter.first) * 100.0f;
            battery = 100.0f - solar;
        }
        MessageOutput.printf("%s Use solar power information: Charger=%0.1f%%, Battery=%0.1f%%\r\n",
            getText(Text::T_HEAD).data(), solar, battery);
    }

    char time[20];
    strftime(time, 20, "%d.%m.%Y %H:%M", &_stage_I_Time_Start);
    MessageOutput.printf("%s Last active time: %s", getText(Text::T_HEAD).data(), time);
    if (_stageIActive) {
        snprintf(time, sizeof(time), " - ongoing");
    } else {
        strftime(time, 20, " - %H:%M", &_stage_I_Time_Stop);
    }
    MessageOutput.printf("%s\r\n", time);

    MessageOutput.printf("%s\r\n", getText(Text::T_HEAD).data());
    MessageOutput.printf("%s 2) Stage-II (Absorption/Float): %s / %s\r\n",
        getText(Text::T_HEAD).data(), (_stageIIEnabled && !_stageIITempOff) ? "Enabled" : "Disabled",
        (_surplusState != State::BULK_POWER && _surplusState != State::IDLE) ? "Active" : "Not active");

    if (_surplusState != State::BULK_POWER && _surplusState != State::IDLE) {
        if (_targetVoltage != 0.0f) {
            MessageOutput.printf("%s Voltage regulation target range: %0.2fV - %0.2fV\r\n",
                getText(Text::T_HEAD).data(), _targetVoltage, _targetVoltage + TARGET_RANGE);
        }

        MessageOutput.printf("%s Regulation power step size: %uW\r\n",
            getText(Text::T_HEAD).data(), _powerStepSize);

        auto qualityAVG = _qualityAVG.getAverage();
        Text text = Text::Q_BAD;
        if (qualityAVG == 0.0f) { text = Text::Q_NODATA; }
        if ((qualityAVG > 0.0f) && (qualityAVG <= 1.1f)) { text = Text::Q_EXCELLENT; }
        if ((qualityAVG > 1.1f) && (qualityAVG <= 1.8f)) { text = Text::Q_GOOD; }
        MessageOutput.printf("%s Regulation quality: %s\r\n",
            getText(Text::T_HEAD).data(), getText(text).data());
        MessageOutput.printf("%s Regulation quality: (Average: %0.2f, Max: %0.0f, Amount: %i)\r\n",
            getText(Text::T_HEAD).data(), _qualityAVG.getAverage(), _qualityAVG.getMax(), _qualityAVG.getCounts());
    }

    strftime(time, 20, "%d.%m.%Y %H:%M", &_stage_II_Time_Start);
    MessageOutput.printf("%s Last active time: %s", getText(Text::T_HEAD).data(), time);
    if (_stageIIActive) {
        snprintf(time, sizeof(time), " - ongoing");
    } else {
        strftime(time, 20, " - %H:%M", &_stage_II_Time_Stop);
    }
    MessageOutput.printf("%s\r\n", time);

    MessageOutput.printf("%s ---------------------------------------------------------------\r\n",
        getText(Text::T_HEAD).data());
    MessageOutput.printf("%s\r\n", getText(Text::T_HEAD).data());
}


/*
 * Returns a string corresponding to the current state
 */
frozen::string const& SurplusPowerClass::getStatusText(SurplusPowerClass::State const state)
{
    static const frozen::string missing = "programmer error: missing status text";

    static const frozen::map<State, frozen::string, 7> texts = {
        { State::IDLE, "Idle" },
        { State::TRY_MORE, "Try more power" },
        { State::REDUCE_POWER, "Reduce power" },
        { State::IN_TARGET, "In target range" },
        { State::MAXIMUM_POWER, "Maximum power" },
        { State::KEEP_LAST_POWER, "Keep last power" },
        { State::BULK_POWER, "Reserve battery power" }
    };

    auto iter = texts.find(state);
    if (iter == texts.end()) { return missing; }

    return iter->second;
}


/*
 * Returns a string corresponding to the text index
 */
frozen::string const& SurplusPowerClass::getText(SurplusPowerClass::Text const tNr)
{
    static const frozen::string missing = "programmer error: missing status text";

    static const frozen::map<Text, frozen::string, 7> texts = {
        { Text::Q_NODATA, "Insufficient data" },
        { Text::Q_EXCELLENT, "Excellent" },
        { Text::Q_GOOD, "Good" },
        { Text::Q_BAD, "Bad" },
        { Text::T_HEAD2, "[SUP II]"},
        { Text::T_HEAD1, "[SUP I]"},
        { Text::T_HEAD, "[SUP]"}
    };

    auto iter = texts.find(tNr);
    if (iter == texts.end()) { return missing; }

    return iter->second;
}


/*
 * Returns a string corresponding to the exit reason
 */
frozen::string const& SurplusPowerClass::getExitText(SurplusPowerClass::ExitState status)
{
    static const frozen::string missing = "programmer error: missing status text";

    static const frozen::map<ExitState, frozen::string, 6> texts = {
        { ExitState::OK_STAGE_I, "Stage-I" },
        { ExitState::OK_STAGE_II, "Stage-II" },
        { ExitState::ERR_TIME, "Error, local time or sunset time not available" },
        { ExitState::ERR_CHARGER, "Error, solar charger data is not available" },
        { ExitState::ERR_BATTERY, "Error, battery data is not available" },
        { ExitState::ERR_SOLAR_POWER, "Error, solar power data is not available" },
    };

    auto iter = texts.find(status);
    if (iter == texts.end()) { return missing; }

    return iter->second;
}
