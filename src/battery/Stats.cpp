// SPDX-License-Identifier: GPL-2.0-or-later
#include <limits>
#include <battery/Stats.h>
#include <Configuration.h>
#include <MqttSettings.h>
#include "Utils.h"

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

    if (_oSoCFullTime.has_value()) {
        tm fullTime;
        time_t nowTime;
        localtime_r(&_oSoCFullTime.value(), &fullTime);
        fullTime.tm_hour = fullTime.tm_min = fullTime.tm_sec = 0; // always start from midnight
        Utils::getEpoch(&nowTime, 5);
        auto days = static_cast<uint16_t>(difftime(nowTime, mktime(&fullTime)) / (60.0 * 60.0 * 24.0));
        addLiveViewValue(root, "fullyChargedDays", days, "days", 0);
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
}

void Stats::checkFullyChargedTime(void)
{
    time_t fulltime;
    if (isSoCValid() && (getSoCAgeSeconds() <= 30) && (getSoC() >= 100.0f) && Utils::getEpoch(&fulltime, 5)) {
        _oSoCFullTime = fulltime;
    }
}

} // namespace Batteries
