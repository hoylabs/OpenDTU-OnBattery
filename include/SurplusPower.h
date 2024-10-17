#pragma once

#include <Arduino.h>
#include <frozen/string.h>
#include "Statistic.h"


class SurplusPowerClass {
    public:
        SurplusPowerClass() = default;
        ~SurplusPowerClass() = default;

        bool useSurplusPower(void) const;
        int32_t calcSurplusPower(int32_t const requestedPower);

    private:
        enum class SurplusState : uint8_t {
            IDLE = 0,
            TRY_MORE = 1,
            REDUCE_POWER = 2,
            IN_TARGET = 3,
            MAXIMUM_POWER = 4,
            KEEP_LAST_POWER = 5
        };

        enum class Text : uint8_t {
            Q_NODATA = 0,
            Q_EXCELLENT = 1,
            Q_GOOD = 2,
            Q_BAD = 3,
            T_HEAD = 4
        };

        frozen::string const& getStatusText(SurplusPowerClass::SurplusState state);
        frozen::string const& getText(SurplusPowerClass::Text tNr);
        void handleQualityCounter(void);

        // to handle regulation
        SurplusState _surplusState = SurplusState::IDLE;    // actual regulation state
        float _absorptionVoltage = -1.0f;                   // from MPPT
        float _floatVoltage = -1.0f;                        // from MPPT
        int32_t _powerStep = 50;                            // power step size in W (default)
        int32_t _surplusPower = 0;                          // actual surplus power
        int32_t _inTargetTime = 0;                          // records the time we hit the target power
        WeightedAVG<float> _avgMPPTVoltage {3};             // the average helps to smooth the regulation

        // to handle the quality counter
        int8_t _qualityCounter = 0;                         // quality counter
        WeightedAVG<float> _qualityAVG {20};                // quality counter average
        int32_t _lastAddPower = 0;                          // last power step
};

extern SurplusPowerClass SurplusPower;
