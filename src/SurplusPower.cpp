/* Surplus-Power-Mode
 *
 * The Surplus-Power-Mode regulates the inverter output power based on the surplus solar power.
 * Surplus solar power is available when the battery is (almost) full and the available
 * solar power is higher than the power consumed in the household.
 *
 * Basic principle:
 * In absorption- and float-mode the MPPT acts like a constant voltage source with current limiter.
 * In that modes we do not get any reliable maximum power or maximum current information from the MPPT.
 * To find the maximum solar power we regulate the inverter power, we go higher and higher, step by step,
 * until we reach the current limit and the voltage begins to drop. When we go one step back and check if
 * the voltage is back above the target voltage. A kind of simple approximation control.
 *
 * Notes:
 * We need Victron VE.Direct Rx/Tx (text-mode and hex-mode) to get all necessary data for the regulation
 *
 * 2024.08.10 - 1.0 - first version
 *
 */
#include "Configuration.h"
#include "VictronMppt.h"
#include "MessageOutput.h"
#include "SurplusPower.h"



#define DELTA_VOLTAGE 0.10f             // we allow some difference between absorption voltage and target voltage
#define MAX_STEPS 20                    // amount of power steps for the approximation
#define TIME_OUT 60000                  // 60 sec regulation step-up timeout
#define MODE_ABSORPTION 4               // MPPT in absorption mode
#define MODE_FLOAT 5                    // MPPT in float mode
#define NOT_VALID -1.0f                 // if data is not available for any reason


SurplusPowerClass SurplusPower;


/*
 * useSurplusPower()
 * check if surplus-power-mode is enabled
 * return:  true = enabled, false = not enabled
 */
bool SurplusPowerClass::useSurplusPower(void) const {

    return (Configuration.get().PowerLimiter.SurplusPowerEnabled);
}


/*
 * handleQualityCounter()
 * check if the regulation quality counter can be added to the quality statistic
 */
void SurplusPowerClass::handleQualityCounter(void) {
    if (_qualityCounter != 0) {
        _qualityAVG.addNumber(_qualityCounter);
    }
    _qualityCounter = 0;
}


/*
 * calcSurplusPower()
 * calculates the surplus power if MPPT indicates absorption or float mode
 * requested power: the power based on actual calculation from "Zero feed throttle" or "Solar-Passthrough"
 * return:          maximum power ("actual solar power" or "requested power")
 */
int32_t SurplusPowerClass::calcSurplusPower(int32_t const requestedPower) {

    // MPPT in absorption or float mode?
    auto vStOfOp = VictronMppt.getStateOfOperation();
    if ((vStOfOp != MODE_ABSORPTION) && (vStOfOp != MODE_FLOAT)) {
        _surplusPower = 0.0;
        _surplusState = SurplusState::IDLE;
        return requestedPower;
    }

    // get the absorption/float voltage from MPPT
    auto xVoltage = VictronMppt.getVoltage(VictronMpptClass::MPPTVoltage::ABSORPTION);
    if (xVoltage != NOT_VALID) { _absorptionVoltage = xVoltage; }
    xVoltage = VictronMppt.getVoltage(VictronMpptClass::MPPTVoltage::FLOAT);
    if (xVoltage != NOT_VALID) { _floatVoltage = xVoltage; }
    if ((_absorptionVoltage == NOT_VALID) || (_floatVoltage == NOT_VALID)) {
        MessageOutput.printf("%s Error, absorption/float voltage from MPPT is not available\r\n",
        getText(Text::T_HEAD).data());
        return requestedPower;
    }

    // get the battery voltage from MPPT
    // Note: like the MPPT we use the MPPT voltage and not the voltage from the battery for regulation
    auto mpptVoltage = VictronMppt.getVoltage(VictronMpptClass::MPPTVoltage::BATTERY);
    if (mpptVoltage == NOT_VALID) {
        MessageOutput.printf("%s Error, battery voltage from MPPT is not available\r\n",
        getText(Text::T_HEAD).data());
        return requestedPower;
    }
    _avgMPPTVoltage.addNumber(mpptVoltage);
    auto avgMPPTVoltage = _avgMPPTVoltage.getAverage();

    // set the regulation target voltage threshold
    auto targetVoltage = (vStOfOp == MODE_ABSORPTION) ? _absorptionVoltage - DELTA_VOLTAGE: _floatVoltage - DELTA_VOLTAGE;

    // state machine: hold, increase or decrease the surplus power
    CONFIG_T& config = Configuration.get();
    int32_t addPower {0};
    switch (_surplusState) {

        case SurplusState::IDLE:
            // start check if all necessary information is available
            _powerStep = config.PowerLimiter.UpperPowerLimit / MAX_STEPS;
            _surplusPower = requestedPower;
            _surplusState = SurplusState::TRY_MORE;
            _qualityCounter = 0;
            _qualityAVG.reset();
            break;

            case SurplusState::KEEP_LAST_POWER:
            // during last regulation step the requested power was higher as the surplus power
            if (mpptVoltage >= targetVoltage) {
                // again above the target voltage, we try to increase the power
                _surplusState = SurplusState::TRY_MORE;
                addPower = _powerStep;
            } else {
                // below the target voltage, we keep the last surplus power
                // but we change the state
                _surplusState = SurplusState::REDUCE_POWER;
            }
            break;

        case SurplusState::TRY_MORE:
            if (mpptVoltage >= targetVoltage) {
                // still above the target voltage, we try to increase the power
                addPower = 2*_powerStep;
            } else {
                // below the target voltage, we need less power
                addPower = -_powerStep;     // less power
                _surplusState = SurplusState::REDUCE_POWER;
            }
            break;

        case SurplusState::REDUCE_POWER:
            if (mpptVoltage >= targetVoltage) {
                // we are in target and can keep the last surplus power value
                _inTargetTime = millis();
                _surplusState = SurplusState::IN_TARGET;
            } else {
                // still below the target voltage, we need less power
                addPower = -_powerStep;
            }
            break;

        case SurplusState::MAXIMUM_POWER:
        case SurplusState::IN_TARGET:
            // note: here we use both the actual and the average battery voltage
            if ((avgMPPTVoltage >= targetVoltage) || (mpptVoltage >= targetVoltage)) {
                // we try to increase the power after a time out of 60 sec
                if ((millis() - _inTargetTime) > TIME_OUT) {
                    addPower = _powerStep;   // try if more power is possible
                    _surplusState = SurplusState::TRY_MORE;
                }
                // we reached the target and can check, how many polarity changes we needed
                handleQualityCounter();
            } else {
                // out of the target voltage we must reduce the power
                addPower = -_powerStep;
                _surplusState = SurplusState::REDUCE_POWER;
            }
            break;

        default:
                addPower = 0;
                _surplusState = SurplusState::IDLE;

    }
    _surplusPower += addPower;

    // we do not go above the maximum power limit
    if (_surplusPower > config.PowerLimiter.UpperPowerLimit) {
        _surplusPower = config.PowerLimiter.UpperPowerLimit;
        _surplusState = SurplusState::MAXIMUM_POWER;
    }

    // we do not go below the requested power
    auto backPower = _surplusPower;
    if (requestedPower > _surplusPower) {
        backPower = requestedPower;
        _surplusState = SurplusState::KEEP_LAST_POWER;
    } else {

        // quality check: we count every power step polarity change ( + to -  and - to +)
        // to give an indication of the regulation quality
        if (((_lastAddPower < 0) && (addPower > 0)) || ((_lastAddPower > 0) && (addPower < 0))) {
            _qualityCounter++;
        }
        _lastAddPower = addPower;
    }

    // print some basic information
    auto qualityAVG = _qualityAVG.getAverage();
    Text text = Text::Q_BAD;
    if ((qualityAVG >= 0.0f) && (qualityAVG <= 1.0f)) { text = Text::Q_EXCELLENT; }
    if ((qualityAVG > 1.0f) && (qualityAVG <= 2.0f)) { text = Text::Q_GOOD; }
    MessageOutput.printf("%s State: %s, Quality: %s, Surplus P: %iW, Requested P: %iW, Returned P: %iW\r\n",
        getText(Text::T_HEAD).data(), getStatusText(_surplusState).data(), getText(text).data(),
        _surplusPower, requestedPower, backPower);


    // todo: maybe we can delete some additional informations after the test phase
    if (config.PowerLimiter.VerboseLogging) {
        MessageOutput.printf("%s Target voltage: %0.2fV, Battery voltage: %0.2f, Average battery voltage: %0.3fV\r\n",
            getText(Text::T_HEAD).data(), targetVoltage, mpptVoltage, avgMPPTVoltage);

        MessageOutput.printf("%s Regulation quality: %s, Average: %0.2f (Min: %0.0f, Max: %0.0f, Amount: %i)\r\n",
            getText(Text::T_HEAD).data(), getText(text).data(),
            _qualityAVG.getAverage(), _qualityAVG.getMin(), _qualityAVG.getMax(), _qualityAVG.getCounts());
    }

    return backPower;
}


/*
 * getStatusText()
 * return: string according to current status
 */
frozen::string const& SurplusPowerClass::getStatusText(SurplusPowerClass::SurplusState state)
{
    static const frozen::string missing = "programmer error: missing status text";

    static const frozen::map<SurplusState, frozen::string, 6> texts = {
        { SurplusState::IDLE, "Idle" },
        { SurplusState::TRY_MORE, "Try more power" },
        { SurplusState::REDUCE_POWER, "Reduce power" },
        { SurplusState::IN_TARGET, "In target range" },
        { SurplusState::MAXIMUM_POWER, "Maximum power" },
        { SurplusState::KEEP_LAST_POWER, "Keep last power" }
    };

    auto iter = texts.find(state);
    if (iter == texts.end()) { return missing; }

    return iter->second;
}


/*
 * getText()
 * return: string according to current status
 */
frozen::string const& SurplusPowerClass::getText(SurplusPowerClass::Text tNr)
{
    static const frozen::string missing = "programmer error: missing status text";

    static const frozen::map<Text, frozen::string, 5> texts = {
        { Text::Q_NODATA, "Insufficient data" },
        { Text::Q_EXCELLENT, "Excellent" },
        { Text::Q_GOOD, "Good" },
        { Text::Q_BAD, "Bad" },
        { Text::T_HEAD, "[Surplus-Mode]"}
    };

    auto iter = texts.find(tNr);
    if (iter == texts.end()) { return missing; }

    return iter->second;
}