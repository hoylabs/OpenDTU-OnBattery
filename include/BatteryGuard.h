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
        void updateBatteryValues(float const nowVoltage, float const nowCurrent, uint32_t const millisStamp);
        std::optional<float> getOpenCircuitVoltage(void);
        std::optional<float> getInternalResistance(void) const;

    private:
        void loop(void);
        void slowLoop(void);

        Task _slowLoopTask;                                 // Task
        Task _fastLoopTask;                                 // Task
        bool _verboseLogging = false;                       // Logging On/Off
        bool _verboseReport = false;                        // Report On/Off
        bool _useBatteryGuard = false;                      // "Battery guard" On/Off
        bool _useLowVoltageLimiter = false;                 // "Low voltage power limiter" On/Off
        bool _useRechargeTheBattery = false;                // "Periodically recharge the battery" On/Off
        uint32_t _lastUpdate = 0;                           // last update time stamp


        // used to calculate the "Open circuit voltage"
        enum class Text : uint8_t { Q_NODATA, Q_EXCELLENT, Q_GOOD, Q_BAD, T_HEAD };

        bool calculateOpenCircuitVoltage(float const nowVoltage, float const nowCurrent);
        bool isDataValid() { return (millis() - _battMillis) < 30*1000; }
        void printOpenCircuitVoltageReport(void);
        bool isResolutionOK(void) const;
        frozen::string const& getText(Text tNr);

        float _battVoltage = 0.0f;                          // actual battery voltage [V]
        float _battCurrent = 0.0f;                          // actual battery current [A]
        uint32_t _battMillis = 0;                           // measurement time stamp [millis()]
        WeightedAVG<float> _battVoltageAVG {5};             // average battery voltage [V]
        WeightedAVG<float> _openCircuitVoltageAVG {5};      // average battery open circuit voltage [V]
        float _determinedResolutionV = 0;                   // determined resolution from the battery voltage [V]
        float _determinedResolutionI = 0;                   // determined resolution from the battery current [V]
        WeightedAVG<uint32_t> _determinedPeriod {20};       // determined measurement period [ms]
        size_t _notAvailableCounter = 0;                    // open circuit voltage not available counter


        // used to calculate the "Battery internal resistance"
        bool calculateInternalResistance(float const nowVoltage, float const nowCurrent);

        float _resistanceFromConfig = 0.0f;                 // configured battery resistance [Ohm]
        WeightedAVG<float> _resistanceFromCalcAVG {10};     // calculated battery resistance [Ohm]
        bool _firstOfTwoAvailable = false;                  // true after to got the first of two values
        bool _minMaxAvailable = false;                      // true if minimum and maximum values are available
        bool _triggerEvent = false;                         // true if we have sufficient current change
        std::pair<float,float> _pFirstVolt = {0.0f,0.0f};   // first of two voltages and related current [V,A]
        std::pair<float,float> _pMaxVolt = {0.0f,0.0f};     // maximum voltage and related current [V,A]
        std::pair<float,float> _pMinVolt = {0.0f,0.0f};     // minimum voltage and related current [V,A]
        uint32_t _lastTriggerMillis = 0;                    // last millis from the first min/max values [millis()]

};

extern BatteryGuardClass BatteryGuard;
