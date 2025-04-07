// SPDX-License-Identifier: GPL-2.0-or-later
#include <CRC.h>
#include <solarcharger/smartbufferbatteries/Stats.h>
#include <battery/Controller.h>
#include <battery/zendure/Stats.h>

namespace SolarChargers::SmartBufferBatteries {

std::optional<float> Stats::getOutputPowerWatts() const
{
    float sum = 0;
    bool updated = false;
    for (const auto& [key, device] : _deviceData) {
        for (const auto& [num, mppt] : device->_mpptData) {
            if (!getValueIfNotOutdated(mppt->_lastUpdate, mppt->_power).has_value()) {
                continue;
            }
            sum += mppt->_power;
            updated = true;
        }
    }

    return updated ? std::optional<float>(sum) : std::nullopt;
}

std::optional<float> Stats::getOutputVoltage() const
{
    float minimum = INFINITY;
    for (const auto& [key, device] : _deviceData) {
        for (const auto& [num, mppt] : device->_mpptData) {
            if (!getValueIfNotOutdated(mppt->_lastUpdate, mppt->_voltage).has_value()) {
                continue;
            }
            minimum = std::min(minimum, mppt->_voltage);
        }
    }

    if (minimum == INFINITY) {
        return std::nullopt;
    }

    return std::optional<float>(minimum);
}

std::optional<float> Stats::getValueIfNotOutdated(const uint32_t lastUpdate, const float value) const {
    // never updated or older than 60 seconds
    if (lastUpdate == 0
        || millis() - lastUpdate > 60 * 1000) {
        return std::nullopt;
    }

    return value;
}

void Stats::getLiveViewData(JsonVariant& root, const boolean fullUpdate, const uint32_t lastPublish) const
{
    ::SolarChargers::Stats::getLiveViewData(root, fullUpdate, lastPublish);

    auto age = millis() - _lastUpdate;

    auto hasUpdate = _lastUpdate > 0 && age < millis() - lastPublish;
    if (!fullUpdate && !hasUpdate) { return; }

    for (const auto& [device, deviceData] : _deviceData) {
        auto dev = deviceData->_manufacture + " " + deviceData->_device;
        auto devage = millis() - deviceData->_lastUpdate;

        const JsonObject instance = root["solarcharger"]["instances"][deviceData->_serial].to<JsonObject>();
        instance["data_age_ms"] = devage;
        instance["hide_serial"] = false;
        instance["product_id"] = dev;

        for (const auto& [mppt, mpptData] : deviceData->_mpptData) {
            auto name = String("mppt" + String(mppt));
            const JsonObject output = instance["values"][name.c_str()].to<JsonObject>();
            output["Power"]["v"] = mpptData->_power;
            output["Power"]["u"] = "W";
            output["Power"]["d"] = 1;
            output["Voltage"]["v"] = mpptData->_voltage;
            output["Voltage"]["u"] = "V";
            output["Voltage"]["d"] = 1;
        }
    }
}

void Stats::setMpptVoltage(std::optional<const uint32_t> id, const size_t num, const float voltage, const uint32_t lastUpdate) {
    if (!id.has_value()) {
        return;
    }

    std::shared_ptr<DeviceData> device;
    try
    {
        device = _deviceData.at(*id);
        device->setMpptData(num, lastUpdate, std::nullopt, voltage);
        _lastUpdate = lastUpdate;
        _lastUpdateOutputVoltage = lastUpdate;
    }
    catch(const std::out_of_range& ex) {;}
}

void Stats::setMpptPower(std::optional<const uint32_t> id, const size_t num, const float power, const uint32_t lastUpdate) {
    if (!id.has_value()) {
        return;
    }

    std::shared_ptr<DeviceData> device;
    try
    {
        device = _deviceData.at(*id);
        device->setMpptData(num, lastUpdate, power, std::nullopt);
        _lastUpdate = lastUpdate;
        _lastUpdateOutputPowerWatts = lastUpdate;
    }
    catch(const std::out_of_range& ex)
    {
        return;
    }
}

DeviceData::DeviceData(const String& manufacture, const String& device, const String& serial, const size_t numMppts /* = 0 */)
    : _manufacture(manufacture)
    , _device(device)
    , _serial(serial)
    , _numMppts(numMppts) { }


void DeviceData::setMpptData(const size_t num, const uint32_t lastUpdate, const std::optional<float> power, std::optional<float> voltage) {
    if (num == 0 || num > _numMppts) {
        return;
    }

    if (!power && !voltage) {
        return;
    }

    std::shared_ptr<MpptData> mppt;
    try
    {
        mppt = _mpptData.at(num);
    }
    catch(const std::out_of_range& ex)
    {
        mppt = std::make_shared<MpptData>();
        _mpptData[num] = mppt;
    }

    _lastUpdate = lastUpdate;
    mppt->_lastUpdate = lastUpdate;

    if (power.has_value()) {
        mppt->_power = *power;
    }

    if (voltage.has_value()) {
        mppt->_voltage = *voltage;
    }

}

std::optional<uint32_t> Stats::addDevice(std::optional<String> const& manufacture, std::optional<String> const& device, std::optional<String> const& serial, const size_t numMppts) {
    if (numMppts < 1 || !serial || !device || !manufacture) {
        return std::nullopt;
    }

    // calculate CRC32 of device data to generate an (almost) unique identifier to be used as key in the map
    const String name = *manufacture + *device + *serial + String(numMppts);
    CRC32 crc(CRC32_POLYNOME, CRC32_INITIAL, CRC32_XOR_OUT, false, false);
    crc.add(reinterpret_cast<const uint8_t*>(name.c_str()), name.length());
    const uint32_t hash = crc.calc();

    // check if the device already exits
    try
    {
        auto d = _deviceData.at(hash);

        // if device found is the same, return the hash
        if (d->_numMppts == numMppts &&
            d->_serial == *serial &&
            d->_manufacture == *manufacture &&
            d->_device == *device
        ) {
            return hash;
        }

        // otherwise remove from map
        _deviceData.erase(hash);
    }
    catch(const std::out_of_range& ex) {;}

    // if no device found for our "hash", add a new one
    _deviceData[hash] = std::make_shared<DeviceData>(*manufacture, *device, *serial, numMppts);
    return hash;
}

bool Stats::hasDevice(std::optional<const uint32_t> id) {
    return id.has_value() ? _deviceData.count(*id) : false;
}

}; // namespace SolarChargers::SmartBufferBatteries
