// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <Arduino.h>
#include <frozen/string.h>
#include <TaskSchedulerDeclarations.h>
#include "Statistic.h"


class BatteryGuardClass {
    public:
        BatteryGuardClass() = default;
        ~BatteryGuardClass() = default;
        BatteryGuardClass(const BatteryGuardClass&) = delete;
        BatteryGuardClass& operator=(const BatteryGuardClass&) = delete;
        BatteryGuardClass(BatteryGuardClass&&) = delete;
        BatteryGuardClass& operator=(BatteryGuardClass&&) = delete;

        void init(Scheduler& scheduler);
        void updateSettings(void);
        void updateBatteryValues(float const nowVoltage, float const nowCurrent, uint32_t const millisCurrent);
        bool isInternalResistanceCalculated(void) const;
        std::optional<float> getOpenCircuitVoltage(void);
        std::optional<float> getInternalResistance(void) const;

        size_t getResistanceCalculationCount() const { return _resistanceFromCalcAVG.getCounts(); }
        const char* getResistanceCalculationState() const { return getResistanceStateText(_rStateMax).data(); }
        float getVoltageResolution() const { return _analyzedResolutionV; }
        float getCurrentResolution() const { return _analyzedResolutionI; }
        float getMeasurementPeriod() const { return _analyzedPeriod.getAverage(); }
        float getVIStampDelay() const { return _analyzedUIDelay.getAverage(); }
        bool isResolutionOK(void) const;

    private:
        void loop(void);
        void slowLoop(void);

        Task _slowLoopTask;                                 // Task (print the report)
        Task _fastLoopTask;                                 // Task (get the battery values)
        bool _verboseLogging = false;                       // Logging On/Off
        bool _useBatteryGuard = false;                      // "Battery guard" On/Off


        struct Data { float value; uint32_t timeStamp; bool valid; };
        Data _i1Data {0.0f, 0, false };                     // buffer the last current data [current, millis(), true/false]
        Data _u1Data {0.0f, 0, false };                     // buffer the last voltage data [voltage, millis(), true/false]

        // used to calculate the "Open circuit voltage"
        enum class Text : uint8_t { Q_NODATA, Q_EXCELLENT, Q_GOOD, Q_BAD, T_HEAD };

        void calculateOpenCircuitVoltage(float const nowVoltage, float const nowCurrent);
        bool isDataValid() const { return (millis() - _battMillis) < 30*1000; }
        void printOpenCircuitVoltageReport(void);
        frozen::string const& getText(Text tNr) const;

        float _battVoltage = 0.0f;                          // actual battery voltage [V]
        float _battCurrent = 0.0f;                          // actual battery current [A]
        uint32_t _battMillis = 0;                           // measurement time stamp [millis()]
        WeightedAVG<float> _battVoltageAVG {5};             // average battery voltage [V]
        WeightedAVG<float> _openCircuitVoltageAVG {5};      // average battery open circuit voltage [V]
        float _analyzedResolutionV = 0;                     // resolution from the battery voltage [V]
        float _analyzedResolutionI = 0;                     // resolution from the battery current [V]
        WeightedAVG<float> _analyzedPeriod {20};            // measurement period [ms]
        WeightedAVG<float> _analyzedUIDelay {20};           // delay between voltage and current [ms]
        size_t _notAvailableCounter = 0;                    // open circuit voltage not available counter


        // used to calculate the "Battery internal resistance"
        enum class RState : uint8_t { IDLE, RESOLUTION, TIME, FIRST_PAIR, TRIGGER, SECOND_PAIR, DELTA_POWER, TOO_BAD, CALCULATED };

        void calculateInternalResistance(float const nowVoltage, float const nowCurrent);
        frozen::string const& getResistanceStateText(RState state) const;

        RState _rState = RState::IDLE;                      // holds the actual calculation state
        RState _rStateMax = RState::IDLE;                   // holds the maximum calculation state
        float _resistanceFromConfig = 0.0f;                 // configured battery resistance [Ohm]
        WeightedAVG<float> _resistanceFromCalcAVG {10};     // calculated battery resistance [Ohm]
        bool _firstOfTwoAvailable = false;                  // true after to got the first of two values
        bool _minMaxAvailable = false;                      // true if minimum and maximum values are available
        bool _triggerEvent = false;                         // true if we have sufficient current change
        std::pair<float,float> _pFirstVolt = {0.0f,0.0f};   // first of two voltages and related current [V,A]
        std::pair<float,float> _pMaxVolt = {0.0f,0.0f};     // maximum voltage and related current [V,A]
        std::pair<float,float> _pMinVolt = {0.0f,0.0f};     // minimum voltage and related current [V,A]
        uint32_t _lastTriggerMillis = 0;                    // last millis from the first min/max values [millis()]
        uint32_t _lastDataInMillis = 0;                     // last millis for data in [millis()]

};

extern BatteryGuardClass BatteryGuard;
