#pragma once

#include <Arduino.h>
#include <frozen/string.h>
#include "Statistic.h"


class SurplusPowerClass {
    public:
        SurplusPowerClass() { updateSettings(); };
        ~SurplusPowerClass() = default;

        bool isSurplusEnabled(void) const;
        uint16_t calculateSurplusPower(uint16_t const requestedPower);
        void updateSettings(void);

        // can be used to temporary disable surplus-power
        enum class Switch : uint8_t {
            STAGE_I_ON = 0,
            STAGE_I_OFF = 1,
            STAGE_I_ASK = 2,
            STAGE_II_ON = 3,
            STAGE_II_OFF = 4,
            STAGE_II_ASK = 5
        };
        bool switchSurplusOnOff(SurplusPowerClass::Switch const onoff);

    private:
        enum class State : uint8_t {
            IDLE = 0,
            TRY_MORE = 1,
            REDUCE_POWER = 2,
            IN_TARGET = 3,
            MAXIMUM_POWER = 4,
            KEEP_LAST_POWER = 5,
            BULK_POWER = 6
        };

        enum class Text : uint8_t {
            Q_NODATA = 0,
            Q_EXCELLENT = 1,
            Q_GOOD = 2,
            Q_BAD = 3,
            T_HEAD2 = 4,
            T_HEAD1 = 5,
            T_HEAD = 6
        };

        frozen::string const& getStatusText(SurplusPowerClass::State const state);
        frozen::string const& getText(SurplusPowerClass::Text const tNr);
        uint16_t calcBulkMode(uint16_t const requestedPower);
        uint16_t calcAbsorptionFloatMode(uint16_t const requestedPower, uint8_t const modeMppt);
        int16_t getTimeToSunset(void);

        // to handle regulation in absorption- and float-mode
        bool _stageIIEnabled = false;                       // surplus-stage-II enable / disable
        bool _stageIITempOff = false;                       // used for temporary deactivation
        State _surplusState = State::IDLE;                  // state machine
        int32_t _surplusPower = 0;                          // actual surplus power [W]
        int16_t _powerStepSize = 0;                         // approximation step size [W]
        uint32_t _lastInTargetMillis = 0;                   // last millis we hit the target
        uint32_t _lastCalcMillis = 0;                       // last millis we calculated the surplus power
        int32_t _surplusUpperPowerLimit = 0;                // inverter upper power limit [W]
        WeightedAVG<float> _avgMPPTVoltage {5};             // the average helps to smooth the regulation [V]

        // to handle the quality counter
        int8_t _qualityCounter = 0;                         // quality counter
        WeightedAVG<float> _qualityAVG {20};                // quality counter average
        int16_t _lastAddPower = 0;                          // last power step
        uint32_t _overruleCounter = 0;                      // counts how often the voltage regulation was overruled by battery current
        uint32_t _errorCounter = 0;                         // counts all errors

        // to handle bulk mode
        bool _stageIEnabled = false;                        // surplus-stage-I enable / disable
        bool _stageITempOff = false;                        // used for temporary deactivation
        int32_t _batteryReserve = 0;                        // battery reserve power [W]
        float _batterySafetyPercent = 0.0f;                 // battery reserve power safety factor [%] (20.0 = 20%)
        int32_t _batteryCapacity = 0;                       // battery capacity [Wh]
        int16_t _durationAbsorptionToSunset = 0;            // time between absorption start and sunset [minutes]
        int16_t _durationNowToAbsorption = 0;               // time from now to start of absorption [minutes]
        int32_t _solarPower = 0;                            // solar panel power [W]
        float _startSoC = 0.0f;                             // start SoC [%] (85.0 = 85%)
        uint32_t _lastReserveCalcMillis = 0;                // last millis we calculated the battery reserve
        WeightedAVG<float> _avgCellVoltage {20};            // in bulk mode we get better results with higher average [V]

};

extern SurplusPowerClass SurplusPower;
