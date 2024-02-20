// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "MqttHandlePowerLimiterHass.h"
#include "Configuration.h"
#include "MqttSettings.h"
#include "NetworkSettings.h"
#include "MessageOutput.h"
#include "Utils.h"

MqttHandlePowerLimiterHassClass MqttHandlePowerLimiterHass;

void MqttHandlePowerLimiterHassClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&MqttHandlePowerLimiterHassClass::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.enable();
}

void MqttHandlePowerLimiterHassClass::loop()
{
    if (!Configuration.get().PowerLimiter.Enabled) {
        return;
    }
    if (_updateForced) {
        publishConfig();
        _updateForced = false;
    }

    if (MqttSettings.getConnected() && !_wasConnected) {
        // Connection established
        _wasConnected = true;
        publishConfig();
    } else if (!MqttSettings.getConnected() && _wasConnected) {
        // Connection lost
        _wasConnected = false;
    }
}

void MqttHandlePowerLimiterHassClass::forceUpdate()
{
    _updateForced = true;
}

void MqttHandlePowerLimiterHassClass::publishConfig()
{
    if (!Configuration.get().Mqtt.Hass.Enabled) {
        return;
    }

    if (!MqttSettings.getConnected()) {
        return;
    }

    publishSelect("DPL Mode", "mdi:gauge", "config", "mode", "mode");

    publishNumber("DPL battery SoC start threshold", "mdi:battery-charging", "config", "battery_soc_start_threshold", "battery_soc_start_threshold", "%", 0, 100);
    publishNumber("DPL battery SoC stop threshold", "mdi:battery-charging", "config", "battery_soc_stop_threshold", "battery_soc_stop_threshold", "%", 0, 100);
    if (!Configuration.get().Vedirect.Enabled) {
        publishNumber("DPL full solar passthrough SoC", "mdi:transmission-tower-import", "config", "full_solar_passthrough_soc", "full_solar_passthrough_soc", "%", 0, 100);
    }
    /*
    publishNumber("DPL battery voltage start threshold", "mdi:battery-charging", "config", "voltage_start_threshold", "voltage_start_threshold", "V", 0, 100);
    publishNumber("DPL battery voltage stop threshold", "mdi:battery-charging", "config", "voltage_stop_threshold", "voltage_stop_threshold", "V", 0, 100);
    publishNumber("DPL full solar passthrough start voltage", "mdi:transmission-tower-import", "config", "full_solar_passthrough_start_voltage", "full_solar_passthrough_start_voltage", "V", 0, 100);
    publishNumber("DPL full solar passthrough stop voltage", "mdi:transmission-tower-import", "config", "full_solar_passthrough_stop_voltage", "full_solar_passthrough_stop_voltage", "V", 0, 100);
    */

    yield();
}

void MqttHandlePowerLimiterHassClass::publishSelect(
    const char* caption, const char* icon, const char* category,
    const char* commandTopic, const char* stateTopic)
{

    String selectId = caption;
    selectId.replace(" ", "_");
    selectId.toLowerCase();

    const String configTopic = "select/powerlimiter/" + selectId + "/config";

    const String cmdTopic = MqttSettings.getPrefix() + "powerlimiter/cmd/" + commandTopic;
    const String statTopic = MqttSettings.getPrefix() + "powerlimiter/status/" + stateTopic;

    DynamicJsonDocument root(1024);
    if (!Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
        return;
    }

    root["name"] = caption;
    root["uniq_id"] = selectId;
    if (strcmp(icon, "")) {
        root["ic"] = icon;
    }
    root["ent_cat"] = category;
    root["cmd_t"] = cmdTopic;
    root["stat_t"] = statTopic;
    JsonArray options = root.createNestedArray("options");
    options.add("0");
    options.add("1");
    options.add("2");

    JsonObject deviceObj = root.createNestedObject("dev");
    createDeviceInfo(deviceObj);

    String buffer;
    serializeJson(root, buffer);
    publish(configTopic, buffer);
}

void MqttHandlePowerLimiterHassClass::publishNumber(
    const char* caption, const char* icon, const char* category,
    const char* commandTopic, const char* stateTopic, const char* unitOfMeasure,
    const int16_t min, const int16_t max)
{

    String numberId = caption;
    numberId.replace(" ", "_");
    numberId.toLowerCase();

    const String configTopic = "number/powerlimiter/" + numberId + "/config";

    const String cmdTopic = MqttSettings.getPrefix() + "powerlimiter/cmd/" + commandTopic;
    const String statTopic = MqttSettings.getPrefix() + "powerlimiter/status/" + stateTopic;

    DynamicJsonDocument root(1024);
    if (!Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
        return;
    }

    root["name"] = caption;
    root["uniq_id"] = numberId;
    if (strcmp(icon, "")) {
        root["ic"] = icon;
    }
    root["ent_cat"] = category;
    root["cmd_t"] = cmdTopic;
    root["stat_t"] = statTopic;
    root["unit_of_meas"] = unitOfMeasure;
    root["min"] = min;
    root["max"] = max;
    root["mode"] = "box";

    JsonObject deviceObj = root.createNestedObject("dev");
    createDeviceInfo(deviceObj);

    String buffer;
    serializeJson(root, buffer);
    publish(configTopic, buffer);
}

/*
void MqttHandlePowerLimiterHassClass::publishSensor(const char* caption, const char* icon, const char* subTopic, const char* deviceClass, const char* stateClass, const char* unitOfMeasurement )
{
    String sensorId = caption;
    sensorId.replace(" ", "_");
    sensorId.replace(".", "");
    sensorId.replace("(", "");
    sensorId.replace(")", "");
    sensorId.toLowerCase();

    String configTopic = "sensor/powerlimiter/" + sensorId + "/config";

    String statTopic = MqttSettings.getPrefix() + "powerlimiter/status/";
    statTopic.concat(subTopic);

    DynamicJsonDocument root(1024);
    if (!Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
        return;
    }
    root["name"] = caption;
    root["stat_t"] = statTopic;
    root["uniq_id"] = "powerlimiter_" + sensorId;

    if (icon != NULL) {
        root["icon"] = icon;
    }

    if (unitOfMeasurement != NULL) {
        root["unit_of_meas"] = unitOfMeasurement;
    }

    JsonObject deviceObj = root.createNestedObject("dev");
    createDeviceInfo(deviceObj);

    if (Configuration.get().Mqtt.Hass.Expire) {
        root["exp_aft"] = Configuration.get().Mqtt.PublishInterval * 3;
    }
    if (deviceClass != NULL) {
        root["dev_cla"] = deviceClass;
    }
    if (stateClass != NULL) {
        root["stat_cla"] = stateClass;
    }

    char buffer[512];
    serializeJson(root, buffer);
    publish(configTopic, buffer);
}
*/

void MqttHandlePowerLimiterHassClass::createDeviceInfo(JsonObject& object)
{
    object["name"] = "Dynamic Power Limiter";
    object["ids"] = "0001";
    object["cu"] = String("http://") + NetworkSettings.localIP().toString();
    object["mf"] = "OpenDTU";
    object["mdl"] = "OpenDTU";
    object["sw"] = AUTO_GIT_HASH;
}

void MqttHandlePowerLimiterHassClass::publish(const String& subtopic, const String& payload)
{
    String topic = Configuration.get().Mqtt.Hass.Topic;
    topic += subtopic;
    MqttSettings.publishGeneric(topic.c_str(), payload.c_str(), Configuration.get().Mqtt.Hass.Retain);
}
