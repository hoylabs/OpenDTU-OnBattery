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
    root["max_age"] = getMaxAgeSeconds();

    if (isSoCValid()) {
        addLiveViewValue(root, "SoC", _soc, "%", _socPrecision);
    }

    auto days = getPastDaysSinceFullyCharged();
    if (days.has_value()) {
        addLiveViewValue(root, "fullyChargedDays", days.value(), "days", 0);
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

    if (_fullChargeRequest.has_value()) {
        addLiveViewTextValue(root, "fullChargeRequest", *_fullChargeRequest ? "yes" : "no");
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

    if (_fullChargeRequest.has_value()) {
        MqttSettings.publish("battery/charging/fullChargeRequest", String(*_fullChargeRequest));
    }
}

std::optional<uint16_t> Stats::getPastDaysSinceFullyCharged(void) const
{
    if (!_oFullyChargedEpoch.has_value()) { return std::nullopt; }

    time_t nowTime;
    if (!Utils::getEpoch(&nowTime, 5)) { return std::nullopt; }

    tm fullTime{};
    if (localtime_r(&_oFullyChargedEpoch.value(), &fullTime) == nullptr) { return std::nullopt; }

    fullTime.tm_hour = fullTime.tm_min = fullTime.tm_sec = 0; // always start from midnight
    auto midnightEpoch = mktime(&fullTime);
    if ((midnightEpoch != -1) && (nowTime >= midnightEpoch)) {

        // calculate the number of full days since the battery reached full charged
        return static_cast<uint16_t>(difftime(nowTime, midnightEpoch) / (60.0 * 60.0 * 24.0));
    }
    return std::nullopt;
}

void Stats::setFullyChargedEpoch(std::optional<time_t> full)
{
    // we only set the fully charged epoch from runtime if we haven't already set it
    // and we have a valid value
    if (!_dataFromRuntimeProcessed && full.has_value() && !_oFullyChargedEpoch.has_value()) {
        _oFullyChargedEpoch = full;
    }
    _dataFromRuntimeProcessed = true;
}

void Stats::checkFullyChargedEpoch(void)
{
    if (!isSoCValid()) { return; } // fast exit if SoC is not supported or not available
    if (_lastSoCFullEpochUpdate == _lastUpdate) { return; } // fast exit from already processed values

    time_t nowEpoch;
    if (!Utils::getEpoch(&nowEpoch, 0)) { return; } // we need a valid epoch to proceed

    _lastSoCFullEpochUpdate = _lastUpdate;
    if (_oFullyChargedEpoch.has_value()) {

        // the goal is to record the time when we reach 100% SoC, to record the time as long as we have 100% SoC,
        // and to maintain the time until we reach 100% SoC again.
        if ((getSoCAgeSeconds() <= 60) && (getSoC() >= 100.0f) && (nowEpoch > _oFullyChargedEpoch.value())) {
            _oFullyChargedEpoch = nowEpoch;
        }
    } else {

        // if the epoch is still unset after receiving data from the runtime file, set it to the current epoch
        // as fallback to avoid waiting until the next 100% SoC to set the epoch
        // yes, the epoch is incorrect in this case, but it is useful for the initial setup and high level functions
        if (_dataFromRuntimeProcessed) {
            _oFullyChargedEpoch = nowEpoch;
        }
    }
}

} // namespace Batteries
