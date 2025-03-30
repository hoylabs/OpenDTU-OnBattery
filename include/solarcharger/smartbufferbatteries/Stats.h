// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <map>
#include <solarcharger/Stats.h>

namespace SolarChargers::SmartBufferBatteries {

class DeviceData;
class MpptData;

class Stats : public ::SolarChargers::Stats {
friend class Provider;

public:
    // the last time *any* data was updated
    uint32_t getAgeMillis() const final { return millis() - _lastUpdate; }

    std::optional<float> getOutputPowerWatts() const final;
    std::optional<float> getOutputVoltage() const;
    std::optional<uint16_t> getPanelPowerWatts() const { return getOutputPowerWatts(); }
    std::optional<float> getYieldTotal() const { return std::nullopt; }
    std::optional<float> getYieldDay() const { return std::nullopt; }
    std::optional<Stats::StateOfOperation> getStateOfOperation() const { return std::nullopt; }
    std::optional<float> getFloatVoltage() const { return std::nullopt; }
    std::optional<float> getAbsorptionVoltage() const { return std::nullopt; }

    void getLiveViewData(JsonVariant& root, const boolean fullUpdate, const uint32_t lastPublish) const final;

    // no need to republish values received via mqtt
    void mqttPublish() const final {}

    // no need to republish values received via mqtt
    void mqttPublishSensors(const boolean forcePublish) const final {}

    // ToDo @vaterlangen: rework battery interface for pushing updates
    std::optional<uint32_t> addDevice(const String& manufacture, const String& device, const String& serial, const size_t numMppts);
    bool verifyDevice(std::optional<const uint32_t> id, const String& serial);
    bool hasDevice(std::optional<const uint32_t> id);
    void setMpptPower(std::optional<const uint32_t> id, const size_t num, const float power, const uint32_t updated);
    void setMpptVoltage(std::optional<const uint32_t> id, const size_t num, const float voltage, const uint32_t lastUpdate);

private:
    uint32_t _lastUpdate = 0;

    uint32_t _lastUpdateOutputPowerWatts = 0;
    uint32_t _lastUpdateOutputVoltage = 0;

    std::optional<float> getValueIfNotOutdated(const uint32_t lastUpdate, const float value) const;

    std::map<uint32_t, std::shared_ptr<DeviceData>> _deviceData = std::map<uint32_t, std::shared_ptr<DeviceData>>();
};
class DeviceData {
    friend class Stats;

public:
    DeviceData(const String& manufacture, const String& device, const String& serial, const size_t numMppts = 0);

private:
    void setMpptData(const size_t num, const uint32_t lastUpdate, const std::optional<float> power = std::nullopt, std::optional<float> voltage = std::nullopt);

    uint32_t _lastUpdate = 0;
    String _manufacture;
    String _device;
    String _serial;
    size_t _numMppts;

    std::map<size_t, std::shared_ptr<MpptData>> _mpptData = std::map<size_t, std::shared_ptr<MpptData>>();
};

class MpptData {
    friend class Stats;
    friend class DeviceData;

private:
    uint32_t _lastUpdate = 0;
    float _power;
    float _voltage;
};


} // namespace SolarChargers::SmartBufferBatteries
