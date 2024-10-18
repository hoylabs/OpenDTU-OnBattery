// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>

#include "AsyncJson.h"
#include "Arduino.h"
#include "JkBmsDataPoints.h"
#include "VeDirectShuntController.h"
#include <cfloat>
#include <utility>

// mandatory interface for all kinds of batteries
class BatteryStats {
    public:
        String const& getManufacturer() const { return _manufacturer; }

        // the last time *any* data was updated
        uint32_t getAgeSeconds() const { return (millis() - _lastUpdate) / 1000; }
        bool updateAvailable(uint32_t since) const;

        float getSoC() const { return _soc; }
        uint32_t getSoCAgeSeconds() const { return (millis() - _lastUpdateSoC) / 1000; }
        uint8_t getSoCPrecision() const { return _socPrecision; }

        float getVoltage() const { return _voltage; }
        uint32_t getVoltageAgeSeconds() const { return (millis() - _lastUpdateVoltage) / 1000; }

        float getChargeCurrent() const { return _current; };
        uint8_t getChargeCurrentPrecision() const { return _currentPrecision; }

        float getDischargeCurrentLimit() const { return _dischargeCurrentLimit; };
        uint32_t getDischargeCurrentLimitAgeSeconds() const { return (millis() - _lastUpdateDischargeCurrentLimit) / 1000; }

        // convert stats to JSON for web application live view
        virtual void getLiveViewData(JsonVariant& root) const;

        void mqttLoop();

        // the interval at which all battery data will be re-published, even
        // if they did not change. used to calculate Home Assistent expiration.
        virtual uint32_t getMqttFullPublishIntervalMs() const;

        bool isSoCValid() const { return _lastUpdateSoC > 0; }
        bool isVoltageValid() const { return _lastUpdateVoltage > 0; }
        bool isCurrentValid() const { return _lastUpdateCurrent > 0; }
        bool isDischargeCurrentLimitValid() const { return _lastUpdateDischargeCurrentLimit > 0; }

        // returns true if the battery reached a critically low voltage/SoC,
        // such that it is in need of charging to prevent degredation.
        virtual bool getImmediateChargingRequest() const { return false; };

        virtual float getChargeCurrentLimitation() const { return FLT_MAX; };

        virtual bool supportsAlarmsAndWarnings() const { return true; };

    protected:
        virtual void mqttPublish() const;

        void setSoC(float soc, uint8_t precision, uint32_t timestamp) {
            _soc = soc;
            _socPrecision = precision;
            _lastUpdateSoC = _lastUpdate = timestamp;
        }

        void setVoltage(float voltage, uint32_t timestamp) {
            _voltage = voltage;
            _lastUpdateVoltage = _lastUpdate = timestamp;
        }

        void setCurrent(float current, uint8_t precision, uint32_t timestamp) {
            _current = current;
            _currentPrecision = precision;
            _lastUpdateCurrent = _lastUpdate = timestamp;
        }

        void setDischargeCurrentLimit(float dischargeCurrentLimit, uint32_t timestamp) {
            _dischargeCurrentLimit = dischargeCurrentLimit;
            _lastUpdateDischargeCurrentLimit = _lastUpdate = timestamp;
        }

        void setManufacturer(const String& m);

        String _hwversion = "";
        String _fwversion = "";
        String _serial = "";
        uint32_t _lastUpdate = 0;

    private:
        String _manufacturer = "unknown";
        uint32_t _lastMqttPublish = 0;
        float _soc = 0;
        uint8_t _socPrecision = 0; // decimal places
        uint32_t _lastUpdateSoC = 0;
        float _voltage = 0; // total battery pack voltage
        uint32_t _lastUpdateVoltage = 0;

        // total current into (positive) or from (negative)
        // the battery, i.e., the charging current
        float _current = 0;
        uint8_t _currentPrecision = 0; // decimal places
        uint32_t _lastUpdateCurrent = 0;

        float _dischargeCurrentLimit = 0;
        uint32_t _lastUpdateDischargeCurrentLimit = 0;
};

class PylontechBatteryStats : public BatteryStats {
    friend class PylontechCanReceiver;

    public:
        void getLiveViewData(JsonVariant& root) const final;
        void mqttPublish() const final;
        bool getImmediateChargingRequest() const { return _chargeImmediately; } ;
        float getChargeCurrentLimitation() const { return _chargeCurrentLimitation; } ;

    private:
        void setLastUpdate(uint32_t ts) { _lastUpdate = ts; }

        float _chargeVoltage;
        float _chargeCurrentLimitation;
        float _dischargeVoltageLimitation;
        uint16_t _stateOfHealth;
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

        uint8_t _moduleCount;
};

class SBSBatteryStats : public BatteryStats {
    friend class SBSCanReceiver;

    public:
        void getLiveViewData(JsonVariant& root) const final;
        void mqttPublish() const final;
        float getChargeCurrent() const { return _current; } ;
        float getChargeCurrentLimitation() const { return _chargeCurrentLimitation; } ;

    private:
        void setLastUpdate(uint32_t ts) { _lastUpdate = ts; }

        float _chargeVoltage;
        float _chargeCurrentLimitation;
        float _dischargeCurrentLimitation;
        uint16_t _stateOfHealth;
        float _current;
        float _temperature;

        bool _alarmUnderTemperature;
        bool _alarmOverTemperature;
        bool _alarmUnderVoltage;
        bool _alarmOverVoltage;
        bool _alarmBmsInternal;

        bool _warningHighCurrentDischarge;
        bool _warningHighCurrentCharge;

        bool _chargeEnabled;
        bool _dischargeEnabled;
};

class PytesBatteryStats : public BatteryStats {
    friend class PytesCanReceiver;

    public:
        void getLiveViewData(JsonVariant& root) const final;
        void mqttPublish() const final;
        bool getImmediateChargingRequest() const { return _chargeImmediately; };
        float getChargeCurrentLimitation() const { return _chargeCurrentLimit; };

    private:
        void setLastUpdate(uint32_t ts) { _lastUpdate = ts; }
        void updateSerial() {
            if (!_serialPart1.isEmpty() && !_serialPart2.isEmpty()) {
                _serial = _serialPart1 + _serialPart2;
            }
        }

        String _serialPart1 = "";
        String _serialPart2 = "";

        float _chargeVoltageLimit;
        float _chargeCurrentLimit;
        float _dischargeVoltageLimit;

        uint16_t _stateOfHealth;
        int _chargeCycles = -1;
        int _balance = -1;

        float _temperature;

        uint16_t _cellMinMilliVolt;
        uint16_t _cellMaxMilliVolt;
        float _cellMinTemperature;
        float _cellMaxTemperature;

        String _cellMinVoltageName;
        String _cellMaxVoltageName;
        String _cellMinTemperatureName;
        String _cellMaxTemperatureName;

        uint8_t _moduleCountOnline;
        uint8_t _moduleCountOffline;

        uint8_t _moduleCountBlockingCharge;
        uint8_t _moduleCountBlockingDischarge;

        float _totalCapacity;
        float _availableCapacity;
        uint8_t _capacityPrecision = 0; // decimal places

        float _chargedEnergy = -1;
        float _dischargedEnergy = -1;

        bool _alarmUnderVoltage;
        bool _alarmOverVoltage;
        bool _alarmOverCurrentCharge;
        bool _alarmOverCurrentDischarge;
        bool _alarmUnderTemperature;
        bool _alarmOverTemperature;
        bool _alarmUnderTemperatureCharge;
        bool _alarmOverTemperatureCharge;
        bool _alarmInternalFailure;
        bool _alarmCellImbalance;

        bool _warningLowVoltage;
        bool _warningHighVoltage;
        bool _warningHighChargeCurrent;
        bool _warningHighDischargeCurrent;
        bool _warningLowTemperature;
        bool _warningHighTemperature;
        bool _warningLowTemperatureCharge;
        bool _warningHighTemperatureCharge;
        bool _warningInternalFailure;
        bool _warningCellImbalance;

        bool _chargeImmediately;
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

        void updateFrom(VeDirectShuntController::data_t const& shuntData);

    private:
        float _temperature;
        bool _tempPresent;
        uint8_t _chargeCycles;
        uint32_t _timeToGo;
        float _chargedEnergy;
        float _dischargedEnergy;
        int32_t _instantaneousPower;
        float _midpointVoltage;
        float _midpointDeviation;
        float _consumedAmpHours;
        int32_t _lastFullCharge;

        bool _alarmLowVoltage;
        bool _alarmHighVoltage;
        bool _alarmLowSOC;
        bool _alarmLowTemperature;
        bool _alarmHighTemperature;
};

class MqttBatteryStats : public BatteryStats {
    friend class MqttBattery;

    public:
        // since the source of information was MQTT in the first place,
        // we do NOT publish the same data under a different topic.
        void mqttPublish() const final { }

        void getLiveViewData(JsonVariant& root) const final;

        bool supportsAlarmsAndWarnings() const final { return false; }
};

class ZendureBatteryStats : public BatteryStats {
    friend class ZendureBattery;

    enum class State : uint8_t {
        Idle        = 0,
        Charging    = 1,
        Discharging = 2,
        Invalid     = 255
    };

    enum class BypassMode : uint8_t {
        Automatic   = 0,
        AlwaysOff   = 1,
        AlwaysOn    = 2,
        Invalid     = 255
    };

    template <typename T>
    static T stateToString(State state){
        switch (state) {
            case State::Idle:
                return "idle";
            case State::Charging:
                return "charging";
            case State::Discharging:
                return "discharging";
            default:
                return "invalid";
        }
    }
    template <typename T>
    static T bypassModeToString(BypassMode state){
        switch (state) {
            case BypassMode::Automatic:
                return "automatic";
            case BypassMode::AlwaysOff:
                return "alwaysoff";
            case BypassMode::AlwaysOn:
                return "alwayson";
            default:
                return "invalid";
        }
    }
    inline static bool isDischarging(State state){
        return state == State::Discharging;
    }
    inline static bool isCharging(State state){
        return state == State::Charging;
    }

    class PackStats {
        friend class ZendureBatteryStats;
        friend class ZendureBattery;

        public:
            PackStats() {}
            explicit PackStats(String serial) : _serial(serial) {}
            virtual ~PackStats(){ }

            String getSerial() const { return _serial; }

            inline uint8_t getCellCount() const { return _cellCount; }
            inline uint16_t getCapacity() const { return _capacity; }
            inline uint16_t getAvailableCapacity() const { return _capacity_avail; };
            inline String getName() const { return _name; }

            static std::shared_ptr<PackStats> fromSerial(String serial){
                if (serial.length() == 15) {
                    if (serial.startsWith("AO4H")){
                        return std::make_shared<PackStats>(PackStats(serial, "AB1000", 960));
                    }
                    if (serial.startsWith("CO4H")){
                        return std::make_shared<PackStats>(PackStats(serial, "AB2000", 1920));
                    }
                    if (serial.startsWith("R04Y")){
                        return std::make_shared<PackStats>(PackStats(serial, "AIO2400", 2400));
                    }
                    return std::make_shared<PackStats>(PackStats(serial));
                }
                return nullptr;
            };

        protected:
            explicit PackStats(String serial, String name, uint16_t capacity, uint8_t cellCount = 15) :
                _serial(serial), _name(name), _capacity(capacity), _cellCount(cellCount){}
            void setSerial(String serial) { _serial = serial; }
            void setHwVersion(String&& version) { _hwversion = std::move(version); }
            void setFwVersion(String&& version) { _fwversion = std::move(version); }

        private:
            String _serial = String();
            String _name = String("UNKOWN");
            uint16_t _capacity = 0;
            uint8_t _cellCount = 15;

            String _fwversion = String();
            String _hwversion = String();

            uint16_t _cell_voltage_min = 0;
            uint16_t _cell_voltage_max = 0;
            uint16_t _cell_voltage_spread = 0;
            uint16_t _cell_voltage_avg = 0;
            int16_t _cell_temperature_max = 0;

            float _state_of_health = 1;
            uint16_t _capacity_avail = 0;

            float _voltage_total = 0.0;
            float _current = 0.0;
            int16_t _power = 0;
            float _soc_level = 0.0;
            State _state = State::Invalid;

            uint32_t _lastUpdate = 0;
    };

    public:
        ZendureBatteryStats() { setManufacturer("Zendure"); }

        void mqttPublish() const;
        void getLiveViewData(JsonVariant& root) const;

        std::map<size_t, std::shared_ptr<PackStats>> getPackDataList() const { return _packData; }

        bool supportsAlarmsAndWarnings() const final { return false; }

    protected:
        std::optional<std::shared_ptr<ZendureBatteryStats::PackStats> > getPackData(size_t index) const;
        std::optional<std::shared_ptr<ZendureBatteryStats::PackStats> > addPackData(size_t index, String serial);

        uint16_t getCapacity() const { return _capacity; };
        uint16_t getUseableCapacity() const { return _capacity_avail * (static_cast<float>(_soc_max - _soc_min) / 100.0); };

    private:
        void setHwVersion(String&& version) {
            if (!version.isEmpty()){
                _hwversion = _device + " (" + std::move(version) + ")";
            }else{
                _hwversion = _device;
            }
        }
        void setFwVersion(String&& version) { _fwversion = std::move(version); }

        void setSerial(String serial) {
            _serial = serial;
        }
        void setSerial(std::optional<String> serial) {
            if (serial.has_value()){
                setSerial(*serial);
            }
        }

        void setDevice(String&& device) {
            _device = std::move(device);
        }

        String _device = String("Unkown");

        std::map<size_t, std::shared_ptr<PackStats>> _packData = std::map<size_t, std::shared_ptr<PackStats> >();

        int16_t _cellTemperature = 0;
        uint16_t _cellMinMilliVolt = 0;
        uint16_t _cellMaxMilliVolt = 0;
        uint16_t _cellDeltaMilliVolt = 0;
        uint16_t _cellAvgMilliVolt = 0;

        float _soc_max = 0.0;
        float _soc_min = 0.0;

        uint16_t _inverse_max = 0;
        uint16_t _input_limit = 0;
        uint16_t _output_limit = 0;

        float _efficiency = 0.0;
        uint16_t _capacity = 0;
        uint16_t _capacity_avail = 0;

        uint16_t _charge_power = 0;
        uint16_t _discharge_power = 0;
        uint16_t _output_power = 0;
        uint16_t _input_power = 0;
        uint16_t _solar_power_1 = 0;
        uint16_t _solar_power_2 = 0;

        int16_t _remain_out_time = 0;
        int16_t _remain_in_time = 0;

        State _state = State::Invalid;
        uint8_t _num_batteries = 0;
        BypassMode _bypass_mode = BypassMode::Invalid;
        bool _bypass_state = false;
        bool _auto_recover = false;
        bool _heat_state = false;
        bool _auto_shutdown = false;
        bool _buzzer = false;

        std::optional<uint64_t> _last_full_timestamp = std::nullopt;
        std::optional<uint32_t> _last_full_charge_hours = std::nullopt;
        std::optional<uint64_t> _last_empty_timestamp = std::nullopt;
        std::optional<bool>  _charge_through_state = std::nullopt;
};
