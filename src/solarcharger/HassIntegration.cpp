// SPDX-License-Identifier: GPL-2.0-or-later

#include <solarcharger/HassIntegration.h>
#include <Configuration.h>
#include <MqttSettings.h>
#include <MqttHandleHass.h>
#include <Utils.h>
#include <__compiled_constants.h>

namespace SolarChargers {

void HassIntegration::publish(const String& subtopic, const String& payload) const
{
    String topic = Configuration.get().Mqtt.Hass.Topic;
    topic += subtopic;
    MqttSettings.publishGeneric(topic.c_str(), payload.c_str(), Configuration.get().Mqtt.Hass.Retain);
}

} // namespace SolarChargers
