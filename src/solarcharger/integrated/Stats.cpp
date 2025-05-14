// SPDX-License-Identifier: GPL-2.0-or-later
#include <CRC.h>
#include <MqttSettings.h>
#include <solarcharger/integrated/Stats.h>

namespace SolarChargers::Integrated {

std::optional<float> Stats::getOutputPowerWatts() const
{
    std::optional<float> sum = std::nullopt;
    for (const auto& [key, device] : _devices) {
        for (const auto& [num, mppt] : device->_mppts) {
            auto power = getValueIfNotOutdated(mppt->getLastUpdate(), mppt->getPower());
            if (power.has_value()) {
                sum = *power + sum.value_or(0);
            }
        }
    }

    return sum;
}

std::optional<float> Stats::getOutputVoltage() const
{
    std::optional<float> minimum = std::nullopt;
    for (const auto& [key, device] : _devices) {
        for (const auto& [num, mppt] : device->_mppts) {
            auto voltage = getValueIfNotOutdated(mppt->getLastUpdate(), mppt->getVoltage());
            if (voltage.has_value()) {
                 minimum = std::min(minimum.value_or(INFINITY), *voltage);
            }
        }
    }

    return minimum;
}

void Stats::getLiveViewData(JsonVariant& root, const boolean fullUpdate, const uint32_t lastPublish) const
{
    ::SolarChargers::Stats::getLiveViewData(root, fullUpdate, lastPublish);

    auto age = millis() - _lastUpdate;

    auto hasUpdate = _lastUpdate > 0 && age < millis() - lastPublish;
    if (!fullUpdate && !hasUpdate) { return; }

    for (const auto& [hash, device] : _devices) {
        auto devage = millis() - device->getLastUpdate();

        const JsonObject instance = root["solarcharger"]["instances"][device->getSerial()].to<JsonObject>();
        instance["data_age_ms"] = devage;
        instance["hide_serial"] = false;
        instance["product_id"] = device->getName();

        for (const auto& [index, mppt] : device->_mppts) {
            auto name = String("mppt" + String(static_cast<size_t>(index)));
            const JsonObject output = instance["values"][name.c_str()].to<JsonObject>();

            auto power = mppt->getPower();
            if (power.has_value()) {
                output["Power"]["v"] = *power;
                output["Power"]["u"] = "W";
                output["Power"]["d"] = 1;
            }

            auto voltage = mppt->getVoltage();
            if (voltage.has_value()) {
                output["Voltage"]["v"] = *voltage;
                output["Voltage"]["u"] = "V";
                output["Voltage"]["d"] = 1;
            }
        }
    }
}

DeviceData::DeviceData(const String& manufacture, const String& device, const String& serial, const size_t numMppts /* = 0 */)
    : _manufacture(manufacture)
    , _device(device)
    , _serial(serial)
    , _numMppts(numMppts)
{
    // we currently support up to 4 MPPTs
    assert(numMppts <= 4);

    // Set up internal data structure
    if (numMppts >= 1) { _mppts[MPPT::Number_1] = std::make_shared<MpptData>(); }
    if (numMppts >= 2) { _mppts[MPPT::Number_2] = std::make_shared<MpptData>(); }
    if (numMppts >= 3) { _mppts[MPPT::Number_3] = std::make_shared<MpptData>(); }
    if (numMppts >= 4) { _mppts[MPPT::Number_4] = std::make_shared<MpptData>(); }
}

DeviceData::~DeviceData() {
    _mppts.clear();
}

std::optional<std::pair<uint32_t, std::shared_ptr<DeviceData>>> Stats::addDevice(std::optional<String> const& manufacture, std::optional<String> const& device, std::optional<String> const& serial, const size_t numMppts) {
    if (numMppts < 1 || numMppts > 4 || !serial || !device || !manufacture) {
        return std::nullopt;
    }

    // calculate CRC32 of device data to generate an (almost) unique identifier to be used as key in the map
    const String name = *manufacture + *device + *serial + String(numMppts);
    CRC32 crc(CRC32_POLYNOME, CRC32_INITIAL, CRC32_XOR_OUT, false, false);
    crc.add(reinterpret_cast<const uint8_t*>(name.c_str()), name.length());
    const uint32_t hash = crc.calc();

    auto new_device = std::make_shared<DeviceData>(*manufacture, *device, *serial, numMppts);

    // check if the device already exits
    try
    {
        auto old_device = _devices.at(hash);
        if (*old_device == *new_device) {
            return std::make_pair(hash, old_device);
        }

        // otherwise remove from map
        _devices.erase(hash);
    }
    catch(const std::out_of_range& ex) {;}

    // if no device found for our "hash", add a new one
    _devices[hash] = new_device;
    return std::make_pair(hash, new_device);
}

void Stats::mqttPublish() const {
    ::SolarChargers::Stats::mqttPublish();

    for (const auto& [hash, device] : _devices) {
        for (const auto& [index, mppt] : device->_mppts) {
            String prefix = "solarchargers/" + device->getSerial() + "/" + static_cast<uint8_t>(index) + "/";

            auto power = mppt->getPower();
            if (power.has_value()) {
                MqttSettings.publish(prefix + "power", String(*power, 0));
            }

            auto voltage = mppt->getVoltage();
            if (voltage.has_value()) {
                MqttSettings.publish(prefix + "voltage", String(*voltage, 1));
            }
        }
    }
}

}; // namespace SolarChargers::Integrated
