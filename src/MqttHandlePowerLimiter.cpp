// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler, Malte Schmidt and others
 */
#include "MessageOutput.h"
#include "MqttSettings.h"
#include "MqttHandlePowerLimiter.h"
#include "PowerLimiter.h"
#include <ctime>
#include <string>

MqttHandlePowerLimiterClass MqttHandlePowerLimiter;

void MqttHandlePowerLimiterClass::init()
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    String topic = MqttSettings.getPrefix() + "powerlimiter/cmd/mode";
    MqttSettings.subscribe(topic.c_str(), 0, std::bind(&MqttHandlePowerLimiterClass::onCmdMode, this, _1, _2, _3, _4, _5, _6));

    _lastPublish = millis();
}


void MqttHandlePowerLimiterClass::loop()
{
    if (!MqttSettings.getConnected() ) {
        return;
    }

    const CONFIG_T& config = Configuration.get();

    if ((millis() - _lastPublish) > (config.Mqtt_PublishInterval * 1000) ) {
      MqttSettings.publish("powerlimiter/status/mode", String(PowerLimiter.getMode()));

      yield();
      _lastPublish = millis();
    }
}


void MqttHandlePowerLimiterClass::onCmdMode(const espMqttClientTypes::MessageProperties& properties,
        const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total)
{
    const CONFIG_T& config = Configuration.get();

    // ignore messages if PowerLimiter is disabled
    if (!config.PowerLimiter_Enabled) {
        return;
    }

    std::string strValue(reinterpret_cast<const char*>(payload), len);
    int intValue = -1;
    try {
        intValue = std::stoi(strValue);
    }
    catch (std::invalid_argument const& e) {
        MessageOutput.printf("PowerLimiter MQTT handler: cannot parse payload of topic '%s' as int: %s\r\n",
                topic, strValue.c_str());
        return;
    }

    switch (intValue) {
        case 2:
            MessageOutput.println("Power limiter unconditional full solar PT");
            PowerLimiter.setMode(PL_MODE_SOLAR_PT_ONLY);
            break;
        case 1:
            MessageOutput.println("Power limiter disabled (override)");
            PowerLimiter.setMode(PL_MODE_FULL_DISABLE);
            break;
        case 0:
            MessageOutput.println("Power limiter normal operation");
            PowerLimiter.setMode(PL_MODE_ENABLE_NORMAL_OP);
            break;
        default:
            MessageOutput.printf("PowerLimiter - unknown mode %d\r\n", intValue);
            break;
    }
}