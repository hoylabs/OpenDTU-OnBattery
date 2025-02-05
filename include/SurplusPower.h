// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <Arduino.h>
#include <frozen/string.h>
#include <solarcharger/Controller.h>
#include <TaskSchedulerDeclarations.h>
#include "Statistic.h"


class SurplusPowerClass {
    public:
        SurplusPowerClass() = default;
        ~SurplusPowerClass() = default;
        SurplusPowerClass(const SurplusPowerClass&) = delete;
        SurplusPowerClass& operator=(const SurplusPowerClass&) = delete;
        SurplusPowerClass(SurplusPowerClass&&) = delete;
        SurplusPowerClass& operator=(SurplusPowerClass&&) = delete;

        void init(Scheduler& scheduler);
        bool isSurplusEnabled(void) const;
        uint16_t calculateSurplus(uint16_t const requestedPower, uint16_t const nowPower, uint32_t const nowMillis);
        void updateSettings(void);
        std::optional<uint16_t> getUpperPowerLimit(void) const { return _oUpperPowerLimit; }

        // can be used to temporary disable surplus-power
        enum class Switch : uint8_t {
            STAGE_I_ON, STAGE_I_OFF, STAGE_I_ASK, STAGE_II_ON, STAGE_II_OFF, STAGE_II_ASK
        };
        bool switchSurplusOnOff(SurplusPowerClass::Switch const onoff);

    private:
        enum class State : uint8_t {
            IDLE, TRY_MORE, REDUCE_POWER, IN_TARGET, MAXIMUM_POWER, KEEP_LAST_POWER, BULK_POWER
        };

        enum class Text : uint8_t {
            Q_NODATA, Q_EXCELLENT, Q_GOOD, Q_BAD, T_HEAD2, T_HEAD1, T_HEAD
        };

        enum class ExitState : uint8_t {
            ERR_TIME = 0, ERR_CHARGER = 1, ERR_BATTERY = 2, ERR_SOLAR_POWER = 4, OK_STAGE_I, OK_STAGE_II,
        };

        void loop(void);
        frozen::string const& getStatusText(SurplusPowerClass::State const state);
        frozen::string const& getText(SurplusPowerClass::Text const tNr);
        frozen::string const& getExitText(SurplusPowerClass::ExitState status);
        uint16_t calcBulkMode(uint16_t const requestedPower, uint16_t const nowPower, uint32_t const nowMillis);
        uint16_t calcSlopePower(uint16_t const requestedPower, int32_t const surplusPower);
        uint16_t calcAbsorptionFloatMode(uint16_t const requestedPower, SolarChargers::Stats::StateOfOperation const modeMppt, uint32_t const nowMillis);
        uint16_t exitSurplus(uint16_t const requestedPower, uint16_t const exitPower, SurplusPowerClass::ExitState const status);
        int16_t getTimeToSunset(void);
        void triggerStageState(bool stageI, bool stageII);
        void printReport(void);

        bool _verboseLogging = false;                       // Logging On/Off
        uint16_t _lastLoggingPower = 0;                     // buffer the last logged surplus or slope power
        bool _verboseReport = false;                        // Report On/Off
        State _surplusState = State::IDLE;                  // state machine
        Task _loopTask;                                     // Task to print the report
        std::optional<uint16_t> _oUpperPowerLimit;          // todo: maybe not necessary

        // to handle absorption- and float-mode
        bool _stageIIEnabled = false;                       // surplus-stage-II enable / disable
        bool _stageIITempOff = false;                       // used for temporary deactivation
        int32_t _surplusPower = 0;                          // actual surplus power [W]
        int16_t _powerStepSize = 0;                         // approximation step size [W]
        uint32_t _lastInTargetMillis = 0;                   // last millis we hit the target
        uint32_t _lastCalcMillis = 0;                       // last millis we calculated the surplus power
        uint16_t _surplusIIUpperPowerLimit = 0;             // stage-II upper power limit [W]
        WeightedAVG<float> _avgMPPTVoltage {5};             // the average helps to smooth the regulation [V]

        // to handle the quality counter
        int8_t _qualityCounter = 0;                         // quality counter
        WeightedAVG<float> _qualityAVG {20};                // quality counter average
        int16_t _lastAddPower = 0;                          // last power step
        uint16_t _overruleCounter = 0;                      // counts how often the voltage regulation was overruled by battery current

        // to handle bulk mode
        bool _stageIEnabled = false;                        // surplus-stage-I enable / disable
        bool _stageITempOff = false;                        // used for temporary deactivation
        int32_t _batteryReserve = 0;                        // battery reserve power [W]
        float _batterySafetyPercent = 0.0f;                 // battery reserve power safety factor [%] (20.0 = 20%)
        uint16_t _batteryCapacity = 0;                      // battery capacity [Wh]
        int16_t _durationAbsorptionToSunset = 0;            // time between absorption start and sunset [minutes]
        int16_t _durationNowToAbsorption = 0;               // time from now to start of absorption [minutes]
        uint16_t _solarPower = 0;                           // solar panel power [W]
        float _startSoC = 0.0f;                             // start SoC [%] (85.0 = 85%)
        uint32_t _lastReserveCalcMillis = 0;                // last millis we calculated the batter reserve power
        uint16_t _surplusIUpperPowerLimit = 0;              // stage-I upper power limit [W]
        WeightedAVG<float> _avgCellVoltage {20};            // in bulk mode we get better results with higher average [V]

        // to handle the slope power
        bool _slopeModeEnabled = false;                     // slope mode enable / disable
        uint16_t _slopeAddPower = 0;                        // power add on top of the requested power [W]
        uint16_t _slopeFactor = 0;                          // slope decrease factor [W/s]
        int32_t _slopePower = 0;                            // actual slope power [W]
        uint32_t _lastSlopeMillis = 0;                      // last millis we calculated the decrease of the slope power

        // to handle the report
        float _targetVoltage = 0.0f;                        // target voltage [V]
        bool _stageIActive = false;                         // used to avoid retrigger on start and stop time
        bool _stageIIActive = false;                        // used to avoid retrigger on start and stop time
        tm _stage_I_Time_Start;                             // last time we enter stage-I
        tm _stage_I_Time_Stop;                              // last time we exit from stage-I
        tm _stage_II_Time_Start;                            // last time we enter stage-II
        tm _stage_II_Time_Stop;                             // last time we exit from stage-II
        std::array<size_t, 4> _errorCounter {0};            // counts all detected errors
        std::pair<uint16_t, uint16_t> _solarCounter;        // first = inverter - battery, second = solar panel power
};

extern SurplusPowerClass SurplusPower;
