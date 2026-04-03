// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <battery/zendure/Constants.h>
#include <MqttSettings.h>
#include <battery/Stats.h>
#include <map>
#include <optional>
#include <Configuration.h>
#include <frozen/string.h>
#include <frozen/map.h>

namespace Batteries::Zendure {

static constexpr const char* invalid = "invalid";

enum class State : uint8_t {
    Idle        = 0,
    Charging    = 1,
    Discharging = 2,
    Invalid     = 255
};

enum class ChargeThroughState : uint8_t {
    Disabled = 0,   // ChargeThrough is disabled by configuration
    Idle = 1,       // ChargeThrough is in standby
    Soft = 2,       // ChargeThrough trying to charge battery to 100 % during normal operation
    Hard = 3,       // ChargeThrough enforcing battery charging to 100 % while setting OutputLimit to 0 W
    Keep = 4        // ChargeThrough keeping target at 100% till discharing or new day begins
};

static constexpr frozen::map<ChargeThroughState, const char*, 5> _chargeThroughStateStrings {
    { ChargeThroughState::Disabled, "disabled" },
    { ChargeThroughState::Idle,     "idle" },
    { ChargeThroughState::Soft,     "soft" },
    { ChargeThroughState::Hard,     "hard" },
    { ChargeThroughState::Keep,     "keep" }
};

static constexpr frozen::map<BatteryZendureConfig::BypassMode_t, const char*, 3> _bypassModeStrings {
    { BatteryZendureConfig::BypassMode_t::Automatic, "automatic" },
    { BatteryZendureConfig::BypassMode_t::AlwaysOff, "alwaysoff" },
    { BatteryZendureConfig::BypassMode_t::AlwaysOn,  "alwayson" }
};


class PackStats;

class Stats : public ::Batteries::Stats {
    friend class Provider;
    friend class LocalMqttProvider;
    friend class ZendureMqttProvider;

    static String parseVersion(uint32_t version)
    {
        if (version == 0) {
            return String();
        }

        uint8_t major = (version >> 12) & 0xF;
        uint8_t minor = (version >> 8) & 0xF;
        uint8_t bugfix = version & 0xFF;

        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%" PRIu8 ".%" PRIu8 ".%" PRIu8, major, minor, bugfix);
        return String(buffer);
    }

    static const char* chargeThroughStateToString(std::optional<ChargeThroughState> mode) {
        if (!mode.has_value()) {
            return invalid;
        }

        return chargeThroughStateToString(*mode);
    }

    static const char* chargeThroughStateToString(ChargeThroughState mode) {
        if (_chargeThroughStateStrings.contains(mode)) {
            return _chargeThroughStateStrings.at(mode);
        }

        return invalid;
    }

    static std::optional<ChargeThroughState> chargeThroughStateFromString(String value) {
        for (auto entry : _chargeThroughStateStrings) {
            if (String(entry.second) == value) {
                return entry.first;
            }
        }

        return std::nullopt;
    }

    static const char* controlModeToString(uint8_t controlMode) {
        switch (controlMode) {
            case BatteryZendureConfig::ControlMode::ControlModeFull:
                return "full-access";
            case BatteryZendureConfig::ControlMode::ControlModeOnce:
                return "write-once";
            case BatteryZendureConfig::ControlMode::ControlModeReadOnly:
                return "read-only";
            default:
                break;
        }
        return invalid;
    }

    static const char* stateToString(State state) {
        switch (state) {
            case State::Idle:
                return "idle";
            case State::Charging:
                return "charging";
            case State::Discharging:
                return "discharging";
            default:
                break;
        }
        return invalid;
    }

    static const char* stateToString(std::optional<State> state) {
        if (state.has_value()) {
            return stateToString(*state);
        }
        return invalid;
    }

    static const char* bypassModeToString(BatteryZendureConfig::BypassMode_t state) {
        switch (state) {
            case BatteryZendureConfig::BypassMode_t::Automatic:
                return "automatic";
            case BatteryZendureConfig::BypassMode_t::AlwaysOff:
                return "alwaysoff";
            case BatteryZendureConfig::BypassMode_t::AlwaysOn:
                return "alwayson";
            default:
                break;
        }

        return invalid;
    }

    static const char* bypassModeToString(std::optional<BatteryZendureConfig::BypassMode_t> state) {
        if (state.has_value()) {
            return bypassModeToString(*state);
        }
        return invalid;
    }

    inline static bool isDischarging(State state) {
        return state == State::Discharging;
    }

    inline static bool isCharging(State state) {
        return state == State::Charging;
    }

public:
    void getLiveViewData(JsonVariant& root) const final;
    void mqttPublish() const final;

    std::optional<String> getHassDeviceName() const final {
        return String(*getManufacturer() + " " + _device);
    }

    bool supportsAlarmsAndWarnings() const final { return false; }

    std::map<size_t, std::shared_ptr<PackStats>> getPackDataList() const { return _packData; }

    static String parseVersion(std::optional<uint32_t> version)
    {
        return parseVersion(version.value_or(0));
    }
    static State parseState(uint8_t state)
    {
        return state > 2 ? State::Invalid : static_cast<State>(state);
    }

protected:
    std::shared_ptr<PackStats> getPackData(size_t index) const;
    std::shared_ptr<PackStats> addPackData(size_t index, String serial);

    std::optional<uint16_t> getUseableCapacity() const {
        if ((_capacity_avail.has_value() || _capacity.has_value()) && _soc_max.has_value() && _soc_min.has_value()) {
            return _capacity_avail.value_or(*_capacity) * (static_cast<float>(*_soc_max - *_soc_min) / 100.0);
        }
        return std::nullopt;
    };

    void detectDeviceFromSerial(const bool force = false) {
        if (_serial.length() != 15) { return; }
        if (!_device.isEmpty() && !force) { return; }

        if (_serial.startsWith("PO1H")) {
            _device = ZENDURE_HUB1200_NAME;
            return;
        }
        if (_serial.startsWith("HO1H")) {
            _device = ZENDURE_HUB2000_NAME;
            return;
        }
        if (_serial.startsWith("R04Y")) {
            _device = ZENDURE_AIO2400_NAME;
            return;
        }
        if (_serial.startsWith("FE1H")) {
            _device = ZENDURE_ACE1500_NAME;
            return;
        }
        if (_serial.startsWith("EE1L")) {
            _device = ZENDURE_HYPER2000_NAME;
            return;
        }
    };

private:
    void setLastUpdate(uint32_t ts) { _lastUpdate = ts; }

    template<typename T>
    inline static void publish(const String &topic, const T &payload, [[maybe_unused]] const size_t precision = 0) {
        if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
            MqttSettings.publish(topic, String(payload, precision));
            return;
        }

        MqttSettings.publish(topic, String(payload));
    }

    template<typename T>
    inline static void publish(const String &topic, const std::optional<T> &payload, const size_t precision = 0) {
        if (!payload.has_value()) {
            return;
        }

        publish(topic, *payload, precision);
    }

    void setHardwareVersion(std::optional<uint32_t> number) {
        if (_hwversion.isEmpty()) {
            _hwversion = _device;
        }

        if (!number.has_value()) { return; }

        auto version = parseVersion(number);
        if (version.isEmpty()) { return; }

        _hwversion = _device + " (" + version + ")";
    }

    inline void setFirmwareVersion(std::optional<uint32_t> number) {
        if (number.has_value()) {
            _fwversion = parseVersion(number);
        }
    }

    void setSerial(String serial) {
        if (_serial == serial) { return; }
        _serial = serial;
        detectDeviceFromSerial(false);
    }
    inline void setSerial(std::optional<String> serial) {
        if (serial.has_value()) {
            setSerial(*serial);
        }
    }
    inline void setDevice(String&& device) {
        _device = std::move(device);
    }

    inline void setInputPower(const uint16_t power) {
        _input_power = power;

        // if there is no input power, there can't be any charging power, even if the battery reports it - so set it to 0 W to prevent confusion
        if (power == 0) {
            setChargePower(0);
        }
    }

    inline void updateSolarInputPower(std::optional<uint16_t> power = std::nullopt) {
        if (power.has_value()) {
            setInputPower(*power);
            return;
        }
        if (_solar_power_1.has_value() && _solar_power_2.has_value()) {
            setInputPower(*_solar_power_1 + *_solar_power_2);
        }
    }

    inline void setSolarPower1(const uint16_t power) {
        _solar_power_1 = power;
        updateSolarInputPower();
    }

    inline void setSolarPower2(const uint16_t power) {
        _solar_power_2 = power;
        updateSolarInputPower();
    }

    void setChargePower(const uint16_t power) {
        _charge_power = power;

        if (power > 0 && (_capacity_avail.has_value() || _capacity.has_value())) {
            _remain_in_time = static_cast<uint16_t>(_capacity_avail.value_or(*_capacity) * (_soc_max.value_or(100.0) - getSoC()) / 100 / static_cast<float>(power) * 60);
            _remain_out_time.reset();
        } else {
            _remain_in_time.reset();
        }
    }

    void setDischargePower(const uint16_t power){
        _discharge_power = power;

        if (power > 0 && (_capacity_avail.has_value() || _capacity.has_value())) {
            _remain_out_time = static_cast<uint16_t>(_capacity_avail.value_or(*_capacity) * (getSoC() - _soc_min.value_or(0.0)) / 100 / static_cast<float>(power) * 60);
            _remain_in_time.reset();
        } else {
            _remain_out_time.reset();
        }
    }

    inline void setOutputPower(const uint16_t power) {
        _output_power = power;

        // if there is no output power, there can't be any discharge power, even if the battery reports it - so set it to 0 W to prevent confusion
        if (power == 0) {
            setDischargePower(0);
        }
    }

    inline void setOutputLimit(const uint16_t power) {
        _output_limit = power;
    }

    inline void setSocMin(const float soc) {
        // Limit value to 0...60% as Zendure seems to do so, too
        if (soc < 0 || soc > 60) {
            return;
        }
        _soc_min = soc;
    }

    inline void setSocMax(const float soc) {
        // Limit value to 40...100% as Zendure seems to do so, too
        if (soc < 40 || soc > 100) {
            return;
        }
        _soc_max = soc;
    }

    inline void setAutoRecover(const uint8_t value) {
        _auto_recover = static_cast<bool>(value);
    }

    inline void setVoltage(float voltage, uint32_t timestamp) {
        if (voltage > 0 && _inverse_max.has_value()) {
            setDischargeCurrentLimit(static_cast<float>(*_inverse_max) / voltage, timestamp);
        }
        Batteries::Stats::setVoltage(voltage, timestamp);
    }

    inline void setState(std::optional<uint8_t> number) {
        if (!number.has_value()) { return; }

        _state = parseState(*number);
    }
    inline void setState(std::optional<State> state) {
        if (!state.has_value()) { return; }

        _state = *state;
    }

    String _device = String();

    std::map<size_t, std::shared_ptr<PackStats>> _packData = std::map<size_t, std::shared_ptr<PackStats> >();

    std::optional<float> _cellTemperature = std::nullopt;
    std::optional<uint16_t> _cellMinMilliVolt = std::nullopt;
    std::optional<uint16_t> _cellMaxMilliVolt = std::nullopt;
    std::optional<uint16_t> _cellDeltaMilliVolt = std::nullopt;
    std::optional<uint16_t> _cellAvgMilliVolt = std::nullopt;

    std::optional<float> _soc_max = std::nullopt;
    std::optional<float> _soc_min = std::nullopt;

    std::optional<uint16_t> _inverse_max = std::nullopt;
    std::optional<uint16_t> _input_limit = std::nullopt;
    std::optional<uint16_t> _output_limit = std::nullopt;

    std::optional<float> _efficiency = std::nullopt;
    std::optional<uint16_t> _capacity = std::nullopt;
    std::optional<uint16_t> _capacity_avail = std::nullopt;

    std::optional<uint16_t> _charge_power = std::nullopt;
    std::optional<uint16_t> _discharge_power = std::nullopt;
    std::optional<uint16_t> _output_power = std::nullopt;
    std::optional<uint16_t> _input_power = std::nullopt;
    std::optional<uint16_t> _solar_power_1 = std::nullopt;
    std::optional<uint16_t> _solar_power_2 = std::nullopt;

    uint16_t _charge_power_cycle = 0;
    uint16_t _discharge_power_cycle = 0;
    uint16_t _output_power_cycle = 0;
    uint16_t _input_power_cycle = 0;
    uint16_t _solar_power_1_cycle = 0;
    uint16_t _solar_power_2_cycle = 0;

    std::optional<uint16_t> _remain_out_time = std::nullopt;
    std::optional<uint16_t> _remain_in_time = std::nullopt;

    std::optional<State> _state = std::nullopt;
    std::optional<uint8_t> _num_batteries = std::nullopt;
    std::optional<BatteryZendureConfig::BypassMode_t> _bypass_mode = std::nullopt;
    std::optional<bool> _bypass_state = std::nullopt;
    std::optional<bool> _auto_recover = std::nullopt;
    std::optional<bool> _heat_state = std::nullopt;
    std::optional<bool> _auto_shutdown = std::nullopt;
    std::optional<bool> _buzzer = std::nullopt;

    std::optional<uint64_t> _last_full_timestamp = std::nullopt;
    std::optional<uint32_t> _last_full_hours = std::nullopt;
    std::optional<uint64_t> _last_empty_timestamp = std::nullopt;
    std::optional<uint32_t> _last_empty_hours = std::nullopt;
    std::optional<ChargeThroughState>  _charge_through_state = std::nullopt;

    std::optional<float> _inaccurateSoC= std::nullopt;
    uint32_t _inaccurateSoCTimestamp = 0;
    bool _hasAccurateSoC = false;

    bool _reachable = false;
};

class PackStats {
    friend class Stats;
    friend class Provider;
    friend class LocalMqttProvider;
    friend class ZendureMqttProvider;

    public:
        PackStats() {}
        explicit PackStats(String serial) : _serial(serial) {}
        virtual ~PackStats() {}

        String getSerial() const { return _serial; }

        inline uint8_t getCellCount() const { return _cellCount; }
        inline String getName() const { return _name; }

        static std::shared_ptr<PackStats> fromSerial(String serial) {
            if (serial.length() == 15) {
                if (serial.startsWith("AO4H")) {
                    return std::make_shared<PackStats>(PackStats(serial, "AB1000", 960));
                }
                if (serial.startsWith("BO4N")) {
                    return std::make_shared<PackStats>(PackStats(serial, "AB1000S", 960));
                }
                if (serial.startsWith("CO4H")) {
                    return std::make_shared<PackStats>(PackStats(serial, "AB2000", 1920));
                }
                if (serial.startsWith("CO4F")) {
                    return std::make_shared<PackStats>(PackStats(serial, "AB2000S", 1920));
                }
                if (serial.startsWith("CO4E")) {
                    return std::make_shared<PackStats>(PackStats(serial, "AB2000X", 1920));
                }
                if (serial.startsWith("ABB3")) {
                    return std::make_shared<PackStats>(PackStats(serial, "AIO2400", 2400));
                }
                return std::make_shared<PackStats>(PackStats(serial));
            }
            return nullptr;
        };

    protected:

        explicit PackStats(String serial, String name, uint16_t capacity, uint8_t cellCount = 15) :
            _serial(serial), _name(name), _capacity(capacity), _cellCount(cellCount) {}

        void setSerial(String serial) { _serial = serial; }
        void setHwVersion(String&& version) { _hwversion = std::move(version); }
        void setFwVersion(String&& version) { _fwversion = std::move(version); }

        void setSoH(std::optional<uint16_t> promille){
            if (!promille.has_value()) {
                return;
            }
            _state_of_health = static_cast<float>(*promille) / 10.0;

            if (!_capacity.has_value()) { return; }
            _capacity_avail = static_cast<float>(*_capacity) * static_cast<float>(*promille) / 1000.0;
        }

    private:
        inline void calcCellVoltageSpread() {
            if (!_cell_voltage_min.has_value() || !_cell_voltage_max.has_value()) { return; }

            _cell_voltage_spread = *_cell_voltage_max - *_cell_voltage_min;
        }
        inline void setPower(std::optional<int16_t> watts = std::nullopt) {
            if (watts.has_value()) {
                _power = *watts;
                _power_calc = false;
            }
            if (!_current_calc && _current.has_value() && _voltage_total.has_value()) {
                _power = static_cast<int16_t>(std::round(*_current * *_voltage_total));
                _power_calc = true;
            }
        }
        inline void setCellVoltageMin(std::optional<uint16_t> deciVolt) {
            if (!deciVolt.has_value()) { return; }

            _cell_voltage_min = *deciVolt * 10;
            calcCellVoltageSpread();
        }
        inline void setCellVoltageMax(std::optional<uint16_t> deciVolt) {
            if (!deciVolt.has_value()) { return; }

            _cell_voltage_max = *deciVolt * 10;
            calcCellVoltageSpread();
        }
        inline void setSoCPromille(std::optional<uint16_t> promille) {
            if (!promille.has_value()) { return; }

            setSoCPercent(static_cast<float>(*promille) / 10.0);
        }
        inline void setSoCPercent(std::optional<uint16_t> percent) {
            if (!percent.has_value()) { return; }

            _soc_level = *percent;
        }
        inline void setCellTemperatureMaxKelvin(std::optional<float> kelvin) {
            if (!kelvin.has_value()) { return; }

            _cell_temperature_max = *kelvin - 273.15;
        }
        inline void setCellTemperatureMaxCelsius(std::optional<float> celsius) {
            if (!celsius.has_value()) { return; }

            _cell_temperature_max = *celsius;
        }
        inline void setVoltage(std::optional<float> deciVolt) {
            if (!deciVolt.has_value()) { return; }

            _voltage_total = *deciVolt / 100.0;
            _cell_voltage_avg = std::round((*deciVolt * 10) / getCellCount());
        }
        inline void setCurrent(std::optional<float> centiAmpere = std::nullopt) {
            if (centiAmpere.has_value()) {
                _current = *centiAmpere / 10.0;
                _current_calc = false;
            }
            if (!_power_calc && _power.has_value() && _voltage_total.value_or(0) != 0) {
                _power = static_cast<int16_t>(std::round(*_current * *_voltage_total));
                _power_calc = true;
            }
        }
        inline void setState(std::optional<uint8_t> number) {
            if (!number.has_value()) { return; }

            _state = *number > 2 ? State::Invalid : static_cast<State>(*number);
        }
        inline void setState(std::optional<State> state) {
            if (!state.has_value()) { return; }

            _state = *state;
        }
        inline void setFirmwareVersion(std::optional<uint32_t> number) {
            if (!number.has_value()) { return; }

            _fwversion = Stats::parseVersion(number);
        }

        String _serial = String();
        String _name = String();
        std::optional<uint16_t> _capacity = std::nullopt;
        uint8_t _cellCount = 15;
        std::optional<uint16_t> _capacity_avail = std::nullopt;

        String _fwversion = String();
        String _hwversion = String();

        std::optional<uint16_t> _cell_voltage_min = std::nullopt;
        std::optional<uint16_t> _cell_voltage_max = std::nullopt;
        std::optional<uint16_t> _cell_voltage_spread = std::nullopt;
        std::optional<uint16_t> _cell_voltage_avg = std::nullopt;
        std::optional<float> _cell_temperature_max = std::nullopt;

        std::optional<float> _state_of_health = std::nullopt;

        std::optional<float> _voltage_total = std::nullopt;
        std::optional<float> _current = std::nullopt;
        std::optional<int16_t> _power = std::nullopt;
        std::optional<float> _soc_level = std::nullopt;
        std::optional<State> _state = std::nullopt;

        bool _current_calc = false;
        bool _power_calc = false;

        uint32_t _lastUpdate = 0;
};


} // namespace Batteries::Zendure
