// SPDX-License-Identifier: GPL-2.0-or-later
#include <limits>
#include <battery/Stats.h>
#include <Configuration.h>
#include <MqttSettings.h>

namespace Batteries {

void Stats::setManufacturer(const String& m)
{
    String sanitized(m);
    for (int i = 0; i < sanitized.length(); i++) {
        char c = sanitized[i];
        if (c < 0x20 || c >= 0x80) {
            sanitized.remove(i); // Truncate string
            break;
        }
    }
    _oManufacturer = std::move(sanitized);
}

bool Stats::updateAvailable(uint32_t since) const
{
    if (_lastUpdate == 0) { return false; } // no data at all processed yet

    auto constexpr halfOfAllMillis = std::numeric_limits<uint32_t>::max() / 2;
    return (_lastUpdate - since) < halfOfAllMillis;
}

void Stats::getLiveViewData(JsonVariant& root) const
{
    String manufacturer = "unknown";
    if (_oManufacturer.has_value()) { manufacturer = *_oManufacturer; }

    root["manufacturer"] = manufacturer;
    if (!_serial.isEmpty()) {
        root["serial"] = _serial;
    }
    if (!_fwversion.isEmpty()) {
        root["fwversion"] = _fwversion;
    }
    if (!_hwversion.isEmpty()) {
        root["hwversion"] = _hwversion;
    }
    root["data_age"] = getAgeSeconds();

    if (isSoCValid()) {
        addLiveViewValue(root, "SoC", _soc, "%", _socPrecision);
    }

    if (isVoltageValid()) {
        addLiveViewValue(root, "voltage", _voltage, "V", 2);
    }

    if (isCurrentValid()) {
        addLiveViewValue(root, "current", _current, "A", _currentPrecision);
    }

    if (isDischargeCurrentLimitValid()) {
        addLiveViewValue(root, "dischargeCurrentLimitation", _dischargeCurrentLimit, "A", 1);
    }

    if (isChargeCurrentLimitValid()) {
        addLiveViewValue(root, "chargeCurrentLimitation", _chargeCurrentLimit, "A", 1);
    }

    if (getNominalCapacity().has_value()) {
        addLiveViewValue(root, "nominalCapacity", *getNominalCapacity(), "Ah", 0);
    }

    if (getNominalVoltage().has_value()) {
        addLiveViewValue(root, "nominalVoltage", *getNominalVoltage(), "V", 2);
    }

    root["showIssues"] = supportsAlarmsAndWarnings();
}

void Stats::mqttLoop()
{
    auto& config = Configuration.get();

    if (!MqttSettings.getConnected()
            || (millis() - _lastMqttPublish) < (config.Mqtt.PublishInterval * 1000)) {
        return;
    }

    mqttPublish();

    _lastMqttPublish = millis();
}

uint32_t Stats::getMqttFullPublishIntervalMs() const
{
    auto& config = Configuration.get();

    // this is the default interval, see mqttLoop(). mqttPublish()
    // implementations in derived classes may choose to publish some values
    // with a lower frequency and hence implement this method with a different
    // return value.
    return config.Mqtt.PublishInterval * 1000;
}

void Stats::mqttPublish() const
{
    if (_oManufacturer.has_value()) {
        MqttSettings.publish("battery/manufacturer", *_oManufacturer);
    }

    MqttSettings.publish("battery/dataAge", String(getAgeSeconds()));

    if (isSoCValid()) {
        MqttSettings.publish("battery/stateOfCharge", String(_soc));
    }

    if (isVoltageValid()) {
        MqttSettings.publish("battery/voltage", String(_voltage));
    }

    if (isCurrentValid()) {
        MqttSettings.publish("battery/current", String(_current));
    }

    if (isDischargeCurrentLimitValid()) {
        MqttSettings.publish("battery/settings/dischargeCurrentLimitation", String(_dischargeCurrentLimit));
    }

    if (isChargeCurrentLimitValid()) {
        MqttSettings.publish("battery/settings/chargeCurrentLimitation", String(_chargeCurrentLimit));
    }

    if (getNominalCapacity().has_value()) {
        MqttSettings.publish("battery/nominalCapacity", String(*getNominalCapacity()));
    }

    if (getNominalVoltage().has_value()) {
        MqttSettings.publish("battery/nominalVoltage", String(*getNominalVoltage()));
    }
}

/*
 * Returns the nominal capacity of the battery, preferring a value reported by the battery itself (if available),
 * but falling back to a user-configured value if not. If neither is available, returns std::nullopt.
*/
std::optional<uint16_t> Stats::getNominalCapacity() const {
    std::optional<uint16_t> capacity = std::nullopt;

    if (_nominalCapacity > 0) {
        capacity = _nominalCapacity;
    } else if (Configuration.get().Battery.NominalCapacity > 0) {
        capacity = Configuration.get().Battery.NominalCapacity;
    }
    return capacity;
}

/*
 * Returns the nominal voltage of the battery, preferring a value reported by the battery itself (if available),
 * but falling back to a user-configured value if not. If neither is available, returns std::nullopt.
*/
std::optional<float> Stats::getNominalVoltage() const {
    std::optional<float> voltage = std::nullopt;

    if (_nominalVoltage > 0) {
        voltage = _nominalVoltage;
    } else if (Configuration.get().Battery.NominalVoltage > 0) {
        voltage = Configuration.get().Battery.NominalVoltage;
    }
    return voltage;
}

} // namespace Batteries
