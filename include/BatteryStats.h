// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>

#include "AsyncJson.h"
#include "Arduino.h"
#include "JkBmsDataPoints.h"
#include "VeDirectShuntController.h"

// mandatory interface for all kinds of batteries
class BatteryStats {
    public:
        String const& getManufacturer() const { return _manufacturer; }

        // the last time *any* datum was updated
        uint32_t getAgeSeconds() const { return (millis() - _lastUpdate) / 1000; }
        bool updateAvailable(uint32_t since) const { return _lastUpdate > since; }

        uint8_t getSoC() const { return _SoC; }
        uint32_t getSoCAgeSeconds() const { return (millis() - _lastUpdateSoC) / 1000; }

        // convert stats to JSON for web application live view
        virtual void getLiveViewData(JsonVariant& root) const;

        void mqttLoop();

        // the interval at which all battery datums will be re-published, even
        // if they did not change. used to calculate Home Assistent expiration.
        virtual uint32_t getMqttFullPublishIntervalMs() const;

        bool isValid() const { return _lastUpdateSoC > 0 && _lastUpdate > 0; }

    protected:
        virtual void mqttPublish() const;

        String _manufacturer = "unknown";
        uint8_t _SoC = 0;
        uint32_t _lastUpdateSoC = 0;
        uint32_t _lastUpdate = 0;

    private:
        uint32_t _lastMqttPublish = 0;
};

class PylontechBatteryStats : public BatteryStats {
    friend class PylontechCanReceiver;

    public:
        void getLiveViewData(JsonVariant& root) const final;
        void mqttPublish() const final;

    private:
        void setManufacturer(String&& m) { _manufacturer = std::move(m); }
        void setSoC(uint8_t SoC) { _SoC = SoC; _lastUpdateSoC = millis(); }
        void setLastUpdate(uint32_t ts) { _lastUpdate = ts; }

        float _chargeVoltage;
        float _chargeCurrentLimitation;
        float _dischargeCurrentLimitation;
        uint16_t _stateOfHealth;
        float _voltage; // total voltage of the battery pack
        // total current into (positive) or from (negative)
        // the battery, i.e., the charging current
        float _current;
        float _temperature;

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
        bool _chargeImmediately;
};

class DalyBatteryStats : public BatteryStats {
    friend class DalyBms;

    public:
        void getLiveViewData(JsonVariant& root) const final;
        void mqttPublish() const final;

    private:
        void setManufacturer(String&& m) { _manufacturer = std::move(m); }
        void setSoC(uint8_t SoC) { _SoC = SoC; _lastUpdateSoC = millis(); }
        void setLastUpdate(uint32_t ts) { _lastUpdate = ts; }

        float _voltage;
        float _current;
        float _temperature;
        float _maxCellmV;
        uint8_t _maxCellVNum;
        float _minCellmV;
        uint8_t _minCellVNum;
        float _cellDiff;
        std::string _dischargechargemosstate;
        uint8_t _numberOfCells;
        uint8_t _numOfTempSensors;
        uint8_t _chargeState;
        uint8_t _loadState;
        uint8_t _chargeFetState;
        uint8_t _dischargeFetState;
        int _bmsHeartBeat;
        float _resCapacityAh;
        int _bmsCycles;
        float _cellVmV[48];
        uint8_t _cellTemperature[16];
        bool _cellBalanceState[48];
        bool _cellBalanceActive;
        bool _connectionState;
};

class JkBmsBatteryStats : public BatteryStats {
    public:
        void getLiveViewData(JsonVariant& root) const final {
            getJsonData(root, false);
        }

        void getInfoViewData(JsonVariant& root) const {
            getJsonData(root, true);
        }

        void mqttPublish() const final;

        uint32_t getMqttFullPublishIntervalMs() const final { return 60 * 1000; }

        void updateFrom(JkBms::DataPointContainer const& dp);

    private:
        void getJsonData(JsonVariant& root, bool verbose) const;

        JkBms::DataPointContainer _dataPoints;
        mutable uint32_t _lastMqttPublish = 0;
        mutable uint32_t _lastFullMqttPublish = 0;

        uint16_t _cellMinMilliVolt = 0;
        uint16_t _cellAvgMilliVolt = 0;
        uint16_t _cellMaxMilliVolt = 0;
        uint32_t _cellVoltageTimestamp = 0;
};

class VictronSmartShuntStats : public BatteryStats {
    public:
        void getLiveViewData(JsonVariant& root) const final;
        void mqttPublish() const final;

        void updateFrom(VeDirectShuntController::veShuntStruct const& shuntData);

    private:
        float _voltage;
        float _current;
        float _temperature;
        bool _tempPresent;
        uint8_t _chargeCycles;
        uint32_t _timeToGo;
        float _chargedEnergy;
        float _dischargedEnergy;
        String _modelName;

        bool _alarmLowVoltage;
        bool _alarmHighVoltage;
        bool _alarmLowSOC;
        bool _alarmLowTemperature;
        bool _alarmHighTemperature;
};

class MqttBatteryStats : public BatteryStats {
    public:
        // since the source of information was MQTT in the first place,
        // we do NOT publish the same data under a different topic.
        void mqttPublish() const final { }

        // the SoC is the only interesting value in this case, which is already
        // displayed at the top of the live view. do not generate a card.
        void getLiveViewData(JsonVariant& root) const final { }

        void setSoC(uint8_t SoC) { _SoC = SoC; _lastUpdateSoC = _lastUpdate = millis(); }
};
