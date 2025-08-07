// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <Arduino.h>
#include <frozen/string.h>
#include <solarcharger/Controller.h>
#include <TaskSchedulerDeclarations.h>
#include "Statistic.h"


struct SurplusReport {
    uint16_t surplusPower = 0;          // surplus power [W]
    uint16_t slopePower = 0;            // slope power [W]
    bool stageIActive = false;          // true if stage-I is active
    bool stageIIActive = false;         // true if stage-II is active
    bool slopeActive = false;           // true if slope mode is active
    tm stageITimeStart;                 // last time we enter stage-I
    tm stageITimeStop;                  // last time we exit from stage-I
    tm stageIITimeStart;                // last time we enter stage-II
    tm stageIITimeStop;                 // last time we exit from stage-II
    float qualityCounter = 0.0f;        // quality counter average
    String qualityText;                 // quality as readable text
    String errorText;                   // error as readable text
};

class SurplusClass {
    public:
        SurplusClass() = default;
        ~SurplusClass() = default;
        SurplusClass(const SurplusClass&) = delete;
        SurplusClass& operator=(const SurplusClass&) = delete;
        SurplusClass(SurplusClass&&) = delete;
        SurplusClass& operator=(SurplusClass&&) = delete;

        void init(Scheduler& scheduler);
        bool isSurplusEnabled(void) const { return (_stageIEnabled || _stageIIEnabled); }
        void stopSurplus(void);
        uint16_t calculateSurplus(uint16_t const requestedPower, uint16_t const nowPower, uint32_t const nowMillis);
        void reloadConfig(void);

        // can be used to temporary disable surplus
        enum class Switch : uint8_t { ON, OFF, ASK };
        bool switchSurplusOnOff(SurplusClass::Switch const onoff);

        // getter functions: surplus power, slope power and report data
        uint16_t getSurplusPower() const { return _surplusPower; }
        uint16_t getSlopePower() const { return _slopePower; }
        SurplusReport const& getReportData();
        String getDatumText(tm const& start, tm const& stop) const;

    private:
        enum class State : uint8_t {
            OFF, IDLE, TRY_MORE, WAIT_CHARGING, LIMIT_CHARGING, REDUCE_POWER, IN_TARGET, MAXIMUM_POWER,
            REQUESTED_POWER, BULK_POWER
        };
        enum class ReturnState : uint8_t {
            ERR_TIME = 0, ERR_CHARGER = 1, ERR_BATTERY = 2, ERR_SOLAR_POWER = 3, OK_STAGE_I, OK_STAGE_II
        };
        enum class ErrorState : uint8_t {
            NO_ERROR, NO_DPL, NO_BATTERY_SOC, NO_BATTERY_INVERTER, NO_CHARGER_STATE, NO_BATTERY_CAPACITY
        };
        frozen::string const _missing = "missing text";

        void loop(void);
        frozen::string const& getStatusText(SurplusClass::State const state) const;
        frozen::string const& getExitText(SurplusClass::ReturnState const status) const;
        frozen::string const& getQualityText(float const qNr) const;
        frozen::string const& getErrorText(SurplusClass::ErrorState const status) const;
        void printReport(void);
        uint16_t calcBulkMode(uint16_t const requestedPower, uint16_t const nowPower, uint32_t const nowMillis);
        uint16_t calcSlopePower(uint16_t const requestedPower, int32_t const surplusPower);
        using CHARGER_STATE = SolarChargers::Stats::StateOfOperation;
        uint16_t calcAbsorptionMode(uint16_t const requestedPower, CHARGER_STATE const modeMppt, uint32_t const nowMillis);
        uint16_t returnFromSurplus(uint16_t const requestedPower, uint16_t const exitPower, SurplusClass::ReturnState const status);
        std::optional<uint16_t> getSolarPower(uint16_t const invPower) const;
        int16_t getTimeToSunset(void);
        uint16_t getUpperPowerLimitSum(void) const;
        void triggerStageState(bool stageI, bool stageII);
        ErrorState checkSurplusRequirements(void);
        void resetRuntimeVariables(void);
        void exitSurplus(void);
        bool isSurplusActive(void) const { return (_data.stageIActive || _data.stageIIActive); }

        State _surplusState = State::OFF;                   // state machine
        int32_t _surplusPower = 0;                          // actual surplus power [W]
        uint16_t _surplusUpperPowerLimit = 0;               // upper power limit [W]
        ErrorState _errorState = ErrorState::NO_ERROR;      // error state
        Task _loopTask;                                     // task to print the report
        uint32_t _lastDebugPrint = 0;                       // last millis we printed the debug logging
        uint16_t _lastLoggingPower = 0;                     // the last logged surplus or slope power
        std::array<size_t, 4> _errorCounter {0};            // counts all detected errors

       // to handle stage-I (bulk mode)
        bool _stageIEnabled = false;                        // surplus-stage-I enable / disable
        bool _stageITempOff = false;                        // can be used for temporary deactivation
        float _batterySafetyPercent = 20.0f;                // battery reserve power safety factor [%] (20.0 = 20%)
        int16_t _sunsetSafetyMinutes = 60;                  // time between absorption start and sunset [minutes] (60 = 1h)
        int32_t _batteryReserve = 0;                        // battery reserve power [W]
        uint32_t _lastReserveCalcMillis = 0;                // last millis we calculated the battery reserve power
        WeightedAVG<uint16_t> _avgSolSlow {30};             // the average helps by cloudy weather (4%) [W]
        WeightedAVG<uint16_t> _avgSolFast {5};              // the average helps by cloudy weather (20%) [W]
        uint16_t _solarPowerFiltered = 0;                   // filtered solar power used for calculation [W]

        // to handle time to sunset (bulk mode)
        int16_t _timeToSunset = 0;                          // time to sunset [minutes]
        uint32_t _lastTimeToSunsetMillis = 0;               // last millis we calculated the time to sunset

        // to handle the slope power (bulk mode)
        bool _slopeEnabled = false;                         // slope mode enable / disable
        int16_t _slopeTarget = -20;                         // power target, on top of the requested power [W]
        int16_t _slopeFactor = -10;                         // slope decrease factor [W/s]
        int32_t _slopePower = 0;                            // actual slope power [W]
        uint32_t _slopeLastMillis = 0;                      // last millis we calculated the decrease of the slope power

        // to handle stage-II (absorption- and float-mode)
        bool _stageIIEnabled = false;                       // surplus-stage-II enable / disable
        bool _stageIITempOff = false;                       // can be used for temporary deactivation
        int16_t _powerStepSize = 0;                         // approximation step size [W]
        float _lastBatteryCurrent = 0.0f;                   // last battery current [A]
        uint32_t _lastInTargetMillis = 0;                   // last millis we hit the target
        uint32_t _lastCalcMillis = 0;                       // last millis we calculated the surplus power
        uint32_t _lastUpdate = 0;                           // last millis we updated the battery current
        uint32_t _regulationTime = 5;                       // time to regulate the surplus power [s]
        WeightedAVG<float> _avgTargetCurrent {4};           // average battery current for the target range[A]
        float _lowerTarget = 0.0f;                          // lower target for the battery current target range [A]
        float _upperTarget = 0.0f;                          // upper target for the battery current target range [A]
        float _chargeTarget = 0.0f;                         // target for the battery charge current [A]
        std::pair<uint32_t,float> _pLastI = {0, 0.0f};       // first of two voltages and related current [V,A]

        // to handle the quality counter (absorption- and float-mode)
        int8_t _qualityCounter = 0;                         // quality counter
        WeightedAVG<float> _qualityAVG {10};                // quality counter average
        int16_t _lastAddPower = 0;                          // last power step

        // to handle the report
        SurplusReport _data {};                             // surplus report data
};

extern SurplusClass Surplus;
