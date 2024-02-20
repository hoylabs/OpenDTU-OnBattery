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

void MqttHandlePowerLimiterClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&MqttHandlePowerLimiterClass::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.enable();

    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    String topic = MqttSettings.getPrefix() + "powerlimiter/cmd/mode";
    MqttSettings.subscribe(topic.c_str(), 0, std::bind(&MqttHandlePowerLimiterClass::onCmdMode, this, _1, _2, _3, _4, _5, _6));
    topic = MqttSettings.getPrefix() + "powerlimiter/cmd/battery_soc_start_threshold";
    MqttSettings.subscribe(topic.c_str(), 0, std::bind(&MqttHandlePowerLimiterClass::onCmdThreshold, this, _1, _2, _3, _4, _5, _6));
    topic = MqttSettings.getPrefix() + "powerlimiter/cmd/battery_soc_stop_threshold";
    MqttSettings.subscribe(topic.c_str(), 0, std::bind(&MqttHandlePowerLimiterClass::onCmdThreshold, this, _1, _2, _3, _4, _5, _6));
    topic = MqttSettings.getPrefix() + "powerlimiter/cmd/full_solar_passthrough_soc";
    MqttSettings.subscribe(topic.c_str(), 0, std::bind(&MqttHandlePowerLimiterClass::onCmdThreshold, this, _1, _2, _3, _4, _5, _6));
    topic = MqttSettings.getPrefix() + "powerlimiter/cmd/voltage_start_threshold";
    MqttSettings.subscribe(topic.c_str(), 0, std::bind(&MqttHandlePowerLimiterClass::onCmdThreshold, this, _1, _2, _3, _4, _5, _6));
    topic = MqttSettings.getPrefix() + "powerlimiter/cmd/voltage_stop_threshold";
    MqttSettings.subscribe(topic.c_str(), 0, std::bind(&MqttHandlePowerLimiterClass::onCmdThreshold, this, _1, _2, _3, _4, _5, _6));
    topic = MqttSettings.getPrefix() + "powerlimiter/cmd/full_solar_passthrough_start_voltage";
    MqttSettings.subscribe(topic.c_str(), 0, std::bind(&MqttHandlePowerLimiterClass::onCmdThreshold, this, _1, _2, _3, _4, _5, _6));
    topic = MqttSettings.getPrefix() + "powerlimiter/cmd/full_solar_passthrough_stop_voltage";
    MqttSettings.subscribe(topic.c_str(), 0, std::bind(&MqttHandlePowerLimiterClass::onCmdThreshold, this, _1, _2, _3, _4, _5, _6));

    _lastPublish = millis();
}


void MqttHandlePowerLimiterClass::loop()
{
    std::unique_lock<std::mutex> mqttLock(_mqttMutex);

    const CONFIG_T& config = Configuration.get();

    if (!config.PowerLimiter.Enabled) {
        _mqttCallbacks.clear();
        return;
    }

    for (auto& callback : _mqttCallbacks) { callback(); }
    _mqttCallbacks.clear();

    mqttLock.unlock();

    if (!MqttSettings.getConnected() ) { return; }

    if ((millis() - _lastPublish) > (config.Mqtt.PublishInterval * 1000) ) {
        auto val = static_cast<unsigned>(PowerLimiter.getMode());
        MqttSettings.publish("powerlimiter/status/mode", String(val));
        MqttSettings.publish("powerlimiter/status/battery_soc_start_threshold", String(config.PowerLimiter.BatterySocStartThreshold));
        MqttSettings.publish("powerlimiter/status/battery_soc_stop_threshold", String(config.PowerLimiter.BatterySocStopThreshold));
        MqttSettings.publish("powerlimiter/status/full_solar_passthrough_soc", String(config.PowerLimiter.FullSolarPassThroughSoc));
        MqttSettings.publish("powerlimiter/status/voltage_start_threshold", String(config.PowerLimiter.VoltageStartThreshold));
        MqttSettings.publish("powerlimiter/status/voltage_stop_threshold", String(config.PowerLimiter.VoltageStopThreshold));
        MqttSettings.publish("powerlimiter/status/full_solar_passthrough_start_voltage", String(config.PowerLimiter.FullSolarPassThroughStartVoltage));
        MqttSettings.publish("powerlimiter/status/full_solar_passthrough_stop_voltage", String(config.PowerLimiter.FullSolarPassThroughStopVoltage));

        yield();
        _lastPublish = millis();
    }
}


void MqttHandlePowerLimiterClass::onCmdMode(const espMqttClientTypes::MessageProperties& properties,
        const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total)
{
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

    std::lock_guard<std::mutex> mqttLock(_mqttMutex);

    using Mode = PowerLimiterClass::Mode;
    switch (static_cast<Mode>(intValue)) {
        case Mode::UnconditionalFullSolarPassthrough:
            MessageOutput.println("Power limiter unconditional full solar PT");
            _mqttCallbacks.push_back(std::bind(&PowerLimiterClass::setMode,
                        &PowerLimiter, Mode::UnconditionalFullSolarPassthrough));
            break;
        case Mode::Disabled:
            MessageOutput.println("Power limiter disabled (override)");
            _mqttCallbacks.push_back(std::bind(&PowerLimiterClass::setMode,
                        &PowerLimiter, Mode::Disabled));
            break;
        case Mode::Normal:
            MessageOutput.println("Power limiter normal operation");
            _mqttCallbacks.push_back(std::bind(&PowerLimiterClass::setMode,
                        &PowerLimiter, Mode::Normal));
            break;
        default:
            MessageOutput.printf("PowerLimiter - unknown mode %d\r\n", intValue);
            break;
    }
}

void MqttHandlePowerLimiterClass::onCmdThreshold(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total)
{
    CONFIG_T& config = Configuration.get();

    char token_topic[MQTT_MAX_TOPIC_STRLEN + 40]; // respect all subtopics
    strncpy(token_topic, topic, MQTT_MAX_TOPIC_STRLEN + 40); // convert const char* to char*

    char* powerlimiter;
    char* subtopic;
    char* setting;
    char* rest = &token_topic[strlen(config.Mqtt.Topic)];

    powerlimiter = strtok_r(rest, "/", &rest);
    subtopic = strtok_r(rest, "/", &rest);
    setting = strtok_r(rest, "/", &rest);

    if (powerlimiter == NULL || subtopic == NULL || setting == NULL) {
        return;
    }

    // check if subtopic is unequal cmd
    if (strcmp(powerlimiter, "powerlimiter") || strcmp(subtopic, "cmd")) {
        return;
    }

    char* strlimit = new char[len + 1];
    memcpy(strlimit, payload, len);
    strlimit[len] = '\0';

    float floatValue = -1.0;
    try {
        floatValue = std::stof(strlimit, NULL);
    }
    catch (std::invalid_argument const& e) {
        MessageOutput.printf("PowerLimiter MQTT handler: cannot parse payload of topic '%s' as int: %s\r\n",
                topic, strlimit);
        return;
    }
    delete[] strlimit;

    if (floatValue < 0.0) {
        MessageOutput.printf("MQTT payload < 0 received --> ignoring\r\n");
        return;
    }

    if (!strcmp(setting, "battery_soc_start_threshold")) {
        // Set Battery SoC start threshold
        const int32_t payload_val = static_cast<uint32_t>(floatValue);
        MessageOutput.printf("Setting battery SoC start threshold to: %d %%\r\n", payload_val);
        config.PowerLimiter.BatterySocStartThreshold = payload_val;

    } else if (!strcmp(setting, "battery_soc_stop_threshold")) {
        // Set Battery SoC stop threshold
        const int32_t payload_val = static_cast<uint32_t>(floatValue);
        MessageOutput.printf("Setting battery SoC stop threshold to: %d %%\r\n", payload_val);
        config.PowerLimiter.BatterySocStopThreshold = payload_val;

    } else if (!strcmp(setting, "full_solar_passthrough_soc")) {
        // Set Full solar passthrough SoC
        const int32_t payload_val = static_cast<uint32_t>(floatValue);
        MessageOutput.printf("Setting full solar passthrough SoC to: %d %%\r\n", payload_val);
        config.PowerLimiter.BatterySocStopThreshold = payload_val;

    } else if (!strcmp(setting, "voltage_start_threshold")) {
        // Set Voltage start threshold
        const float payload_val = floatValue;
        MessageOutput.printf("Setting voltage start threshold to: %.2f %%\r\n", payload_val);
        config.PowerLimiter.BatterySocStopThreshold = payload_val;

    } else if (!strcmp(setting, "voltage_stop_threshold")) {
        // Set Voltage stop threshold
        const float payload_val = floatValue;
        MessageOutput.printf("Setting voltage stop threshold to: %.2f %%\r\n", payload_val);
        config.PowerLimiter.BatterySocStopThreshold = payload_val;

    } else if (!strcmp(setting, "full_solar_passthrough_start_voltage")) {
        // Set Full solar passthrough start voltage
        const float payload_val = floatValue;
        MessageOutput.printf("Setting full solar passthrough start voltage to: %.2f %%\r\n", payload_val);
        config.PowerLimiter.BatterySocStopThreshold = payload_val;

    } else if (!strcmp(setting, "full_solar_passthrough_stop_voltage")) {
        // Set Full solar passthrough stop voltage
        const float payload_val = floatValue;
        MessageOutput.printf("Setting full solar passthrough stop voltage to: %.2f %%\r\n", payload_val);
        config.PowerLimiter.BatterySocStopThreshold = payload_val;
    }

    Configuration.write();
}
