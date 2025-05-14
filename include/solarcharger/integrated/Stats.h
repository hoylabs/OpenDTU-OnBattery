// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <map>
#include <solarcharger/Stats.h>
namespace SolarChargers::Integrated {

enum class MPPT : uint8_t {
    Number_1 = 1,
    Number_2 = 2,
    Number_3 = 3,
    Number_4 = 4
};

class MpptData {

public:
    uint32_t getLastUpdate() const {
        return _lastUpdate;
    }

    std::optional<float> getPower() const {
        return _power;
    }

    std::optional<float> getVoltage() const {
        return _voltage;
    }

    void setPower(const float power, const uint32_t ms) {
        _power = power;
        _lastUpdate = ms;
    }

    void setVoltage(const float voltage, const uint32_t ms) {
        _voltage = voltage;
        _lastUpdate = ms;
    }

private:
    uint32_t _lastUpdate = 0;
    std::optional<float> _power   = std::nullopt;
    std::optional<float> _voltage = std::nullopt;
};

class DeviceData {
    friend class Stats;

public:
    DeviceData(const String& manufacture, const String& device, const String& serial, const size_t numMppts = 0);
    virtual ~DeviceData();

    uint32_t getLastUpdate() const {
        uint32_t lastUpdate = 0;
        for (const auto& mppt : _mppts) {
            lastUpdate = std::max(lastUpdate, mppt.second->getLastUpdate());
        }
        return lastUpdate;
    }

    inline bool operator==(const DeviceData& other) {
        if (other._numMppts != _numMppts) { return false; }
        if (other._manufacture != _manufacture) { return false; }
        if (other._device != _device) { return false; }
        if (other._serial != _serial) { return false; }

        return true;
    }

    // bool equals(const String& manufacture, const String& device, const String& serial, const size_t numMppts) {
    //     if (_manufacture != manufacture) { return false; }
    //     if (_device != device) { return false; }
    //     if (_serial != serial) { return false; }
    //     if (_numMppts != numMppts) { return false; }

    //     return true;
    // }
    inline String getName() {
        return _manufacture + " " + _device;
    }

    inline String getManufacture() {
        return _manufacture;
    }

    inline String getDevice() {
        return _device;
    }

    inline String getSerial() {
        return _serial;
    }

    inline const std::map<MPPT, std::shared_ptr<MpptData>> & getMppts() {
        return _mppts;
    }


private:
    String _manufacture;
    String _device;
    String _serial;
    size_t _numMppts;

    std::map<MPPT, std::shared_ptr<MpptData>> _mppts = std::map<MPPT, std::shared_ptr<MpptData>>();
};


class Stats : public ::SolarChargers::Stats {

public:
    virtual ~Stats() {
        _devices.clear();
    }

    // the last time *any* data was updated
    uint32_t getAgeMillis() const final { return millis() - _lastUpdate; }

    std::optional<float> getOutputPowerWatts() const final;
    std::optional<float> getOutputVoltage() const final;
    std::optional<uint16_t> getPanelPowerWatts() const { return getOutputPowerWatts(); }
    std::optional<float> getYieldTotal() const { return std::nullopt; }
    std::optional<float> getYieldDay() const { return std::nullopt; }
    std::optional<Stats::StateOfOperation> getStateOfOperation() const { return std::nullopt; }
    std::optional<float> getFloatVoltage() const { return std::nullopt; }
    std::optional<float> getAbsorptionVoltage() const { return std::nullopt; }

    void getLiveViewData(JsonVariant& root, const boolean fullUpdate, const uint32_t lastPublish) const final;

    // no need to republish values received via mqtt
    void mqttPublish() const final;

    // no need to republish values received via mqtt
    void mqttPublishSensors(const boolean forcePublish) const final {}

    std::optional<std::pair<uint32_t, std::shared_ptr<DeviceData>>> addDevice(std::optional<String> const& manufacture, std::optional<String> const& device, std::optional<String> const& serial, const size_t numMppts);

    inline bool hasDevice(std::optional<const uint32_t> id) const {
        return id.has_value() ? static_cast<bool>(_devices.count(*id)) : false;
    }

    uint32_t getLastUpdate() const {
        uint32_t lastUpdate = 0;
        for (const auto& device : _devices) {
            lastUpdate = std::max(lastUpdate, device.second->getLastUpdate());
        }
        return lastUpdate;
    }

private:
    uint32_t _lastUpdate = 0;

    uint32_t _lastUpdateOutputPowerWatts = 0;
    uint32_t _lastUpdateOutputVoltage = 0;

    inline std::optional<float> getValueIfNotOutdated(const uint32_t lastUpdate, const std::optional<float> value) const {
        return (lastUpdate == 0 || millis() - lastUpdate > 60 * 1000) ? std::nullopt : value;
    }

    std::map<uint32_t, std::shared_ptr<DeviceData>> _devices = std::map<uint32_t, std::shared_ptr<DeviceData>>();
};

} // namespace SolarChargers::Integrated
