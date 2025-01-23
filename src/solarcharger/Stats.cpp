// SPDX-License-Identifier: GPL-2.0-or-later
#include <solarcharger/Stats.h>
#include <Configuration.h>
#include <MqttSettings.h>
#include <PowerLimiter.h>

namespace SolarChargers {

void Stats::getLiveViewData(JsonVariant& root, boolean fullUpdate, uint32_t lastPublish) const
{
    // power limiter state
    root["dpl"]["PLSTATE"] = -1;
    if (Configuration.get().PowerLimiter.Enabled) {
        root["dpl"]["PLSTATE"] = PowerLimiter.getPowerLimiterState();
    }
    root["dpl"]["PLLIMIT"] = PowerLimiter.getInverterOutput();

    root["solarcharger"]["full_update"] = fullUpdate;
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
}

} // namespace SolarChargers
