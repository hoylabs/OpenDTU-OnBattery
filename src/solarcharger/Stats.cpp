// SPDX-License-Identifier: GPL-2.0-or-later
#include <solarcharger/Stats.h>
#include <Configuration.h>
#include <MqttSettings.h>

namespace SolarChargers {

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

} // namespace SolarChargers
