// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <battery/Stats.h>

namespace Batteries::JkBmsCan
{

    class Stats : public ::Batteries::Stats
    {
        friend class Provider;

    public:
        void getLiveViewData(JsonVariant &root) const final;
        void mqttPublish() const final;

        float getChargeCurrentLimitation() const { return _chargeCurrentLimitation; };

        void updateFromV2(uint8_t *rx, uint32_t now);
        void updateFromV1(uint8_t *rx, uint32_t now);
        void evaluateErrors(uint32_t now);

        // Configuration / limits
        static constexpr uint8_t MAX_CELLS = 24;

    private:
        void setLastUpdate(uint32_t ts) { _lastUpdate = ts; }

        float _chargeVoltage;
        float _chargeCurrentLimitation;
        float _dischargeVoltageLimitation;
        uint8_t _stateOfHealth;
        float _minCellTemperature;
        float _maxCellTemperature;
        float _avgCellTemperature;

        float _mosfetTemperature;
        bool _hasMosfetTemperature = false;

        float _cellVoltage[MAX_CELLS];
        float _packVoltage;
        float _MaxCellVoltage;
        uint8_t _MaxCellVoltageNumber;
        float _MinCellVoltage;
        uint8_t _MinCellVoltageNumber;

        float _capacityRemaining;
        float _fullChargeCapacity;
        float _cycleCapacity;
        uint16_t _cycleCount;

        uint32_t _bmsRunTime;
        uint16_t _heaterCurrent;

        void applyV2();
        void applyV1();
        uint8_t getSeverity(uint8_t alarm);

        uint32_t _v2ErrorMask = 0;
        uint32_t _v1SeverityMask = 0;

        uint32_t _lastV2Ts = 0;
        uint32_t _lastV1Ts = 0;

        bool _alarmOverCurrentDischarge;
        bool _alarmOverCurrentCharge;
        bool _alarmUnderTemperature;
        bool _alarmOverTemperature;
        bool _alarmUnderVoltage;
        bool _alarmOverVoltage;
        bool _alarmBmsInternal;

        bool _warningHighCurrentDischarge;
        bool _warningHighCurrentCharge;
        bool _warningLowTemperature;
        bool _warningHighTemperature;
        bool _warningLowVoltage;
        bool _warningHighVoltage;
        bool _warningBmsInternal;

        bool _chargeEnabled;
        bool _dischargeEnabled;
        bool _balanceEnabled;
        bool _heaterEnabled;
        bool _accEnabled;
        bool _chargerPluged;

        bool _chargeRequest;
        bool _chargeAndHeat;

        uint8_t _moduleCount;
        bool _hasV2Frames = false;
    };

} // namespace Batteries::JkBmsCan
