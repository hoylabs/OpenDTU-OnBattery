/* Surplus-Power-Mode
 *
 * The Surplus-Power-Mode regulates the inverter output power based on the surplus solar power.
 * Surplus solar power is available when the battery is almost full and the available
 * solar power is higher than the power consumed in the household.
 * The secondary goal is to fully charge the battery until end of the day.
 *
 * Basic principle of Surplus-Stage-I (MPPT in bulk mode):
 * In bulk mode the MPPT acts like a current source.
 * In this mode we get reliable maximum solar power information from the MPPT and can use it for regulation.
 * We do not use all solar power for the inverter. We must reserve power for the battery to reach absorption
 * mode until end of the day.
 * The calculation of the "reserve power" is based on actual SoC and remaining time to absorption (sunset).
 *
 * Basic principle of Surplus-Stage-II (MPPT in absorption/float mode):
 * In absorption- and float-mode the MPPT acts like a voltage source with current limiter.
 * In these modes we don't get reliable information about the maximum solar power or current from the MPPT.
 * To find the maximum solar power we increase the inverter power, we go higher and higher, step by step,
 * until we reach the solar power limit. On this point the MPPT current limiter will kick in and the voltage
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
 * We need Victron VE.Direct Rx/Tx (text-mode and hex-mode) to get MPPT configured absorption- and
 * float-voltage and the solar panel power.
 *
 * 10.08.2024 - 1.00 - first version, Stage-II (absorption-/float-mode)
 * 30.11.2024 - 1.10 - add of Stage-I (bulk-mode) and minor improvements of Stage-II
 */

#include "Configuration.h"
#include "VictronMppt.h"
#include "MessageOutput.h"
#include "SunPosition.h"
#include "Battery.h"
#include "SurplusPower.h"


// support for debugging, 0 = without extended logging, 1 = with extended logging
constexpr int MODULE_DEBUG = 1;


#define MODE_BULK 3                     // MPPT in bulk mode
#define MODE_ABSORPTION 4               // MPPT in absorption mode
#define MODE_FLOAT 5                    // MPPT in float mode
#define RESERVE_POWER_MAX 99999         // Default value, battery reserve power [W]
#define EFFICIENCY_MPPT 0.97f           // 97%, constant value is good enough for the surplus calculation
#define EFFICIENCY_INVERTER 0.94f       // 94%, constant value is good enough for the surplus calculation


SurplusPowerClass SurplusPower;


/*
 * Update of class parameter used to calculate the surplus power.
 * Must be called after updates of DPL parameter (for example: TotalUpperPowerLimit)
 */
void SurplusPowerClass::updateSettings() {
    auto const& config = Configuration.get();

    // todo: get the parameter for stage-I from the configuration
    _stageIEnabled = false;             // surplus-stage-I (bulk-mode) enable / disable
    _startSoC = 80.0f;                  // [%] stage-I, start SoC
    _batteryCapacity = 2500;            // [Wh] stage-I, battery capacity *** ATTENTION: This value must fit to your system ***
    _batterySafetyPercent = 30.0f;      // [%] stage-I, battery reserve safety factor
    _durationAbsorptionToSunset = 30;   // [Minutes] stage-I, duration from absorption to sunset
    _surplusUpperPowerLimit = 0;        // [W] upper power limit, if 0 we use the DPL total upper power limit

    // todo: get the parameter for stage-II from the configuration
    _stageIIEnabled = true;             // surplus-stage-II (absorption-mode) enable / disable

    // make sure to be inside lower and upper bounds
    // todo: better move to web UI?
    if ((_startSoC < 40.0f) || (_startSoC > 100.0f)) { _startSoC = 70.0f; }
    if ((_batteryCapacity < 100) || (_batteryCapacity > 40000)) { _batteryCapacity = 2500; }
    if ((_batterySafetyPercent < 0.0f) || (_batterySafetyPercent > 100.0f)) { _batterySafetyPercent = 20.0f; }
    if ((_durationAbsorptionToSunset < 0) || (_durationAbsorptionToSunset > 4*60)) { _durationAbsorptionToSunset = 60; }

    // todo: instead of TotalUpperPowerLimit use sum of all battery powered inverters
    if (_surplusUpperPowerLimit == 0) { _surplusUpperPowerLimit = config.PowerLimiter.TotalUpperPowerLimit; }

    // power steps for the approximation regulation in stage-II
    // the power step size should not be below the hysteresis, otherwise one step has no effect
    _powerStepSize = (_surplusUpperPowerLimit) / 20;
    _powerStepSize = std::max(_powerStepSize, static_cast<int16_t>(config.PowerLimiter.TargetPowerConsumptionHysteresis)) + 1;

}


/*
 * Returns the "surplus power" or the "requested power", whichever is higher
 * "surplus power" is the power based on calculation of surplus stage-I or stage-II
 * "requested power" is the power based on calculation of household consumption
 */
uint16_t SurplusPowerClass::calculateSurplusPower(uint16_t const requestedPower) {

    // the regulation loop "Inverter -> MPPT -> Solar Panel -> Measurement" needs time. And different to
    // "Zero-Feed-Throttle" the "Surplus-Mode" is not in a hurry. We always wait 5 sec before
    // we do next calculation. In the meantime we use the value from the last calculation
    if ((millis() - _lastCalcMillis) < 5 * 1000 ) {
        if (_surplusPower <= requestedPower) { return requestedPower; }

        // we just print this message if surplus power is more as requested power
        if (Configuration.get().PowerLimiter.VerboseLogging || MODULE_DEBUG == 1) {
            MessageOutput.printf("%s State: %s, Surplus Power: %iW,  Requested Power: %iW, Returned Power: %iW\r\n",
                getText(Text::T_HEAD).data(), getStatusText(_surplusState).data(),
                _surplusPower, requestedPower, _surplusPower);
        }
        return _surplusPower;
    }
    _lastCalcMillis = millis();

    // we can do nothing if we do not get the actual MPPT operation mode
    auto oState = VictronMppt.getStateOfOperation();
    if (!oState.has_value()) {
        _errorCounter++;
        MessageOutput.printf("%s Error, MPPT operation mode is not available\r\n", getText(Text::T_HEAD).data());
        return requestedPower;
    }
    auto vStOfOp = oState.value();

    // Stage-I enabled and MPPT in bulk mode?
    if (_stageIEnabled && !_stageITempOff && (vStOfOp == MODE_BULK)) {
        return calcBulkMode(requestedPower);
    }

    // Stage-II enabled and MPPT in absorption or float mode?
    if (_stageIIEnabled && !_stageIITempOff && ((vStOfOp == MODE_ABSORPTION) || (vStOfOp == MODE_FLOAT))) {
        return calcAbsorptionFloatMode(requestedPower, vStOfOp);
    }

    // nothing to do, we go into IDLE mode
    _surplusState = State::IDLE;
    _surplusPower = 0;

    // todo: use veMpptStruct::getCsAsString()
    MessageOutput.printf("%s State: %s, Stage-I: %s, Stage-II: %s, MPPT mode: %s\r\n",
        getText(Text::T_HEAD).data(), getStatusText(_surplusState).data(),
        (_stageIEnabled && !_stageITempOff) ? "On" : "Off", (_stageIIEnabled && !_stageIITempOff) ? "On" : "Off",
        (vStOfOp == MODE_BULK) ? "Bulk" : (vStOfOp == MODE_ABSORPTION) ? "Absorption" : (vStOfOp == MODE_FLOAT) ? "Float" : "Off");
    return requestedPower;
}


/*
 * Calculates the surplus-power-stage_II if MPPT indicates absorption or float mode
 * requestedPower:  The power based on actual calculation from "Zero feed throttle"
 * modeAF:          Absorption or float mode
 * return:          The "surplus power" or the "requested power" whichever is higher
 */
uint16_t SurplusPowerClass::calcAbsorptionFloatMode(uint16_t const requestedPower, uint8_t const modeAF) {

    // Note: Actual we use the MPPT voltage to find the maximum available sun power.
    // An alternative way would be to use the "Charger over current" information from the MPPT. (Not tested up to now)

    // get the absorption and float voltage from MPPT
    auto oAbsorptionVoltage = VictronMppt.getVoltage(VictronMpptClass::MPPTVoltage::ABSORPTION);
    auto oFloatVoltage = VictronMppt.getVoltage(VictronMpptClass::MPPTVoltage::FLOAT);
    if ((!oAbsorptionVoltage.has_value()) || (!oFloatVoltage.has_value())) {
        _errorCounter++;
        MessageOutput.printf("%s Error, absorption or float voltage from MPPT is not available\r\n", getText(Text::T_HEAD2).data());
        return requestedPower;
    }

    // set the regulation target voltage threshold
    // we allow 100mV difference between absorption voltage and target voltage
    auto targetVoltage = (modeAF == MODE_ABSORPTION) ? oAbsorptionVoltage.value() : oFloatVoltage.value();
    targetVoltage = targetVoltage / 1000.0f - 0.1f;    // voltage [V]

    // get the actual battery voltage from MPPT
    // Note: Like the MPPT we also use the MPPT voltage and not the voltage from the battery for regulation
    auto oMpptVoltage = VictronMppt.getVoltage(VictronMpptClass::MPPTVoltage::BATTERY);
    if (!oMpptVoltage.has_value()) {
        _errorCounter++;
        MessageOutput.printf("%s Error, battery voltage from MPPT is not available\r\n", getText(Text::T_HEAD2).data());
        return requestedPower;
    }

    // actual MPPT voltage [V] and average MPPT voltage [V]
    auto mpptVoltage = oMpptVoltage.value() / 1000.0f;
    _avgMPPTVoltage.addNumber(mpptVoltage);
    auto avgMPPTVoltage = _avgMPPTVoltage.getAverage();

    // state machine: hold, increase or decrease the surplus power
    int16_t addPower = 0;
    switch (_surplusState) {

        case State::IDLE:
            _errorCounter = 0;
            [[fallthrough]];

        case State::BULK_POWER:
            // if stage-I was active before, we can start stage-II maybe with the identical power
            _surplusPower = std::max(_surplusPower, static_cast<int32_t>(requestedPower));
            _surplusState = State::TRY_MORE;
            _qualityCounter = 0;
            _overruleCounter = 0;
            _qualityAVG.reset();
            break;

        case State::KEEP_LAST_POWER:
            // during last regulation step the requested power was higher as the surplus power
            if (mpptVoltage >= targetVoltage) {
                // again above the target voltage, we try to increase the power
                _surplusState = State::TRY_MORE;
                addPower = _powerStepSize;
            } else {
                // below the target voltage, we keep the last surplus power but change the state
                _surplusState = State::REDUCE_POWER;
            }
            break;

        case State::TRY_MORE:
            if (mpptVoltage >= targetVoltage) {
                // still above the target voltage, we increase the power
                addPower = 2*_powerStepSize;
            } else {
                // below the target voltage, we need less power
                addPower = -_powerStepSize;     // less power
                _surplusState = State::REDUCE_POWER;
            }
            break;

        case State::REDUCE_POWER:
            if (mpptVoltage >= targetVoltage) {
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
            if ((avgMPPTVoltage >= targetVoltage) || (mpptVoltage >= targetVoltage)) {
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

    // if available, we can use the battery current
    auto const& config = Configuration.get();
    if ((addPower >= 0) && (_surplusPower > 0)) {
        auto stats = Battery.getStats();
        if (config.Battery.Enabled && stats->isCurrentValid() && stats->getAgeSeconds() < 5) {
            if (stats->getChargeCurrent() < 0.0f) {
                // overrule voltage regulation if battery current is negative
                addPower = -_powerStepSize;
                _surplusState = State::REDUCE_POWER;
                _overruleCounter++;
            }
        }
    }

    _surplusPower += addPower;

    // we do not go below 0 or above the upper power limit
    _surplusPower = std::max(_surplusPower, 0);
    if (_surplusPower > _surplusUpperPowerLimit) {
        _surplusPower = _surplusUpperPowerLimit;
        _surplusState = State::MAXIMUM_POWER;
    }

    // we do not go below the requested power
    uint16_t backPower = static_cast<uint16_t>(_surplusPower);
    if (requestedPower > backPower) {
        backPower = requestedPower;
        _qualityCounter = 0;
        _surplusState = State::KEEP_LAST_POWER;
    } else {
        // regulation quality: count the polarity changes
        if (((_lastAddPower < 0) && (addPower > 0)) || ((_lastAddPower > 0) && (addPower < 0))) {
            _qualityCounter++;
        }
        _lastAddPower = addPower;
    }

    if (config.PowerLimiter.VerboseLogging || MODULE_DEBUG == 1) {
        MessageOutput.printf("%s State: %s, Surplus power: %iW, Requested power: %iW, Returned power: %iW\r\n",
            getText(Text::T_HEAD2).data(), getStatusText(_surplusState).data(),
            _surplusPower, requestedPower, backPower);

        auto qualityAVG = _qualityAVG.getAverage();
        Text text = Text::Q_BAD;
        if (qualityAVG == 0.0f) { text = Text::Q_NODATA; }
        if ((qualityAVG > 0.0f) && (qualityAVG <= 1.1f)) { text = Text::Q_EXCELLENT; }
        if ((qualityAVG > 1.1f) && (qualityAVG <= 1.8f)) { text = Text::Q_GOOD; }
        MessageOutput.printf("%s Regulation quality: %s, (Average: %0.2f, Min: %0.0f, Max: %0.0f, Amount: %i)\r\n",
            getText(Text::T_HEAD2).data(), getText(text).data(),
            _qualityAVG.getAverage(), _qualityAVG.getMin(), _qualityAVG.getMax(), _qualityAVG.getCounts());

        // todo: maybe we can delete additional information after the test phase
        MessageOutput.printf("%s Target voltage: %0.2fV, Battery voltage: %0.2f, Average battery voltage: %0.3fV\r\n",
            getText(Text::T_HEAD2).data(), targetVoltage, mpptVoltage, avgMPPTVoltage);
        MessageOutput.printf("%s Battery current overrule counter: %i, Error counter: %i\r\n",
            getText(Text::T_HEAD2).data(), _overruleCounter, _errorCounter);
    }

    return backPower;
}


/*
 * Calculates the surplus-power-stage_I if MPPT indicates bulk-mode
 * requestedPower:  The power based on actual calculation of "Zero feed throttle" or "Solar passthrough"
 * return:          The "surplus power" or the "requested power" whichever is higher
 */
uint16_t SurplusPowerClass::calcBulkMode(uint16_t const requestedPower) {

    // prepared for future: these values can also get "start voltage" instead of "start SoC"
    // Note: We also need calculation of actual SoC derived from the voltage before we can offer this
    float startValue = _startSoC;
    float stopValue  = _startSoC - 2.0f;
    float actValue = 0.0f;
    auto const& config = Configuration.get();

    // get the actual SoC from the battery provider
    auto stats = Battery.getStats();
    if (config.Battery.Enabled && stats->isSoCValid() && stats->getSoCAgeSeconds() < 60) {
        actValue = stats->getSoC();
    } else {
        _errorCounter++;
        MessageOutput.printf("%s Error, battery SoC not available\r\n", getText(Text::T_HEAD1).data());
        return requestedPower;
    }

    // below the stop threshold or below start threshold?
    if ((actValue <= stopValue) || ((actValue < startValue) && (_surplusState == State::IDLE))) {
        _surplusPower = 0;
        _surplusState = State::IDLE;

        if (MODULE_DEBUG == 1) {
            MessageOutput.printf("%s State: %s, Actual value: %0.3f, Start value: %0.3f, Stop value: %0.3f\r\n",
                getText(Text::T_HEAD1).data(), getStatusText(_surplusState).data(), actValue, startValue, stopValue);
        }
        return requestedPower;
    }

    // get the solar panel power from MPPTs
    _solarPower = VictronMppt.getPowerOutputWatts();
    if (_solarPower == -1.0f) {
        _errorCounter++;
        MessageOutput.printf("%s Error, solar panel power not available\r\n", getText(Text::T_HEAD1).data());
        return requestedPower;
    }

    if (_surplusState == State::IDLE) {
        // reset some parameter
        _batteryReserve = RESERVE_POWER_MAX;
        _lastReserveCalcMillis = 0;
        _surplusPower = 0;
        _errorCounter = 0;
    }
    _surplusState = State::BULK_POWER;

    // calculate the battery reserve power in a fixed period of 5 min
    // Note: Not necessary to do it more frequently. Saves processing time
    if ((millis() - _lastReserveCalcMillis) > 5 * 60 * 1000) {
        _lastReserveCalcMillis = millis();

        // prepared for future: can also be derived from battery voltage
        auto actSoC = actValue;

        // we calculate the time from now to start of absorption mode in minutes
        _durationNowToAbsorption = getTimeToSunset() - _durationAbsorptionToSunset;

        // calculation of the power, we want to reserve for the battery
        if (_durationNowToAbsorption > 0) {
            // time left, so we can calculate the battery reserve power
            _batteryReserve = _batteryCapacity * (0.998f - actSoC / 100.0f) / _durationNowToAbsorption * 60 *
                (1.0f + _batterySafetyPercent / 100.0f);
            _batteryReserve = std::max(_batteryReserve, 0); // avoid negative values
        } else {
            // time is over, but we did not reach the absorption mode, so we use maximum reserve power
            _batteryReserve = RESERVE_POWER_MAX;
            _durationNowToAbsorption = 0;  // avoid negative values
        }
    }

    // surplus power (inverter AC power) including power loss
    _surplusPower = (_solarPower * EFFICIENCY_MPPT - _batteryReserve) * EFFICIENCY_INVERTER;
    _surplusPower = std::max(_surplusPower, 0); // avoid negative values
    _surplusPower = std::min(_surplusPower, _surplusUpperPowerLimit);

    // we do not go below the requested power
    auto backPower = std::max(static_cast<uint16_t>(_surplusPower), requestedPower);

    if (config.PowerLimiter.VerboseLogging || MODULE_DEBUG == 1) {
        MessageOutput.printf("%s State: %s, Surplus power: %iW,  Requested power: %iW, Returned power: %iW\r\n",
            getText(Text::T_HEAD1).data(), getStatusText(_surplusState).data(),
            _surplusPower, requestedPower, backPower);

        // todo: maybe we can delete some additional informations after the test phase
        MessageOutput.printf("%s Solar power: %iW, Reserved power: %iW, Time to absorption: %02i:%02i, Battery SoC: %0.2f%%\r\n",
            getText(Text::T_HEAD1).data(),
            _solarPower, _batteryReserve, _durationNowToAbsorption / 60, _durationNowToAbsorption % 60, actValue);
        MessageOutput.printf("%s Error counter: %i\r\n",
            getText(Text::T_HEAD1).data(), _errorCounter);
    }

    return backPower;
}


/*
 * Returns time to sunset im minutes if actual time is between 0:00 - sunset
 * or 0 if actual time is between sunset - 24:00
 */
int16_t SurplusPowerClass::getTimeToSunset(void) {
    struct tm actTime, sunsetTime;
    int16_t timeToSunset = 0;

    if (getLocalTime(&actTime, 10) && SunPosition.sunsetTime(&sunsetTime)) {
        timeToSunset = sunsetTime.tm_hour * 60 + sunsetTime.tm_min - actTime.tm_hour * 60 - actTime.tm_min;
        if (timeToSunset < 0) { timeToSunset = 0; }
    } else {
        _errorCounter++;
        MessageOutput.printf("%s Error, local time or sunset time not available\r\n", getText(Text::T_HEAD1).data());
    }

    return timeToSunset;
}


/*
 * Temporary switch-off the "surplus power" for stage-I or stage-II.
 * For example: If a battery manager is on the way to force a fully charge of the battery ;-)
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


/*
 * Returns string according to current status
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
 * Returns string according to text index
 */
frozen::string const& SurplusPowerClass::getText(SurplusPowerClass::Text const tNr)
{
    static const frozen::string missing = "programmer error: missing status text";

    static const frozen::map<Text, frozen::string, 7> texts = {
        { Text::Q_NODATA, "Insufficient data" },
        { Text::Q_EXCELLENT, "Excellent" },
        { Text::Q_GOOD, "Good" },
        { Text::Q_BAD, "Bad" },
        { Text::T_HEAD2, "[Surplus II]"},
        { Text::T_HEAD1, "[Surplus I]"},
        { Text::T_HEAD, "[Surplus]"}
    };

    auto iter = texts.find(tNr);
    if (iter == texts.end()) { return missing; }

    return iter->second;
}
