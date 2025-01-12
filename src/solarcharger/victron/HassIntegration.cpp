// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "Configuration.h"
#include "MqttSettings.h"
#include "MqttHandleHass.h"
#include "Utils.h"
#include "__compiled_constants.h"
#include <solarcharger/victron/HassIntegration.h>

namespace SolarChargers::Victron {

void HassIntegration::publishSensors(const VeDirectMpptController::data_t &mpptData) const
{
    publishSensor("MPPT serial number", "mdi:counter", "SER", nullptr, nullptr, nullptr, mpptData);
    publishSensor("MPPT firmware version integer", "mdi:counter", "FWI", nullptr, nullptr, nullptr, mpptData);
    publishSensor("MPPT firmware version formatted", "mdi:counter", "FWF", nullptr, nullptr, nullptr, mpptData);
    publishSensor("MPPT firmware version FW", "mdi:counter", "FW", nullptr, nullptr, nullptr, mpptData);
    publishSensor("MPPT firmware version FWE", "mdi:counter", "FWE", nullptr, nullptr, nullptr, mpptData);
    publishSensor("MPPT state of operation", "mdi:wrench", "CS", nullptr, nullptr, nullptr, mpptData);
    publishSensor("MPPT error code", "mdi:bell", "ERR", nullptr, nullptr, nullptr, mpptData);
    publishSensor("MPPT off reason", "mdi:wrench", "OR", nullptr, nullptr, nullptr, mpptData);
    publishSensor("MPPT tracker operation mode", "mdi:wrench", "MPPT", nullptr, nullptr, nullptr, mpptData);
    publishSensor("MPPT Day sequence number (0...364)", "mdi:calendar-month-outline", "HSDS", NULL, "total", "d", mpptData);

    // battery info
    publishSensor("Battery voltage", NULL, "V", "voltage", "measurement", "V", mpptData);
    publishSensor("Battery current", NULL, "I", "current", "measurement", "A", mpptData);
    publishSensor("Battery power (calculated)", NULL, "P", "power", "measurement", "W", mpptData);
    publishSensor("Battery efficiency (calculated)", NULL, "E", NULL, "measurement", "%", mpptData);

    // panel info
    publishSensor("Panel voltage", NULL, "VPV", "voltage", "measurement", "V", mpptData);
    publishSensor("Panel current (calculated)", NULL, "IPV", "current", "measurement", "A", mpptData);
    publishSensor("Panel power", NULL, "PPV", "power", "measurement", "W", mpptData);
    publishSensor("Panel yield total", NULL, "H19", "energy", "total_increasing", "kWh", mpptData);
    publishSensor("Panel yield today", NULL, "H20", "energy", "total", "kWh", mpptData);
    publishSensor("Panel maximum power today", NULL, "H21", "power", "measurement", "W", mpptData);
    publishSensor("Panel yield yesterday", NULL, "H22", "energy", "total", "kWh", mpptData);
    publishSensor("Panel maximum power yesterday", NULL, "H23", "power", "measurement", "W", mpptData);

    // optional info, provided only if the charge controller delivers the information
    if (mpptData.relayState_RELAY.first != 0) {
        publishBinarySensor("MPPT error relay state", "mdi:electric-switch", "RELAY", "ON", "OFF", mpptData);
    }
    if (mpptData.loadOutputState_LOAD.first != 0) {
        publishBinarySensor("MPPT load output state", "mdi:export", "LOAD", "ON", "OFF", mpptData);
    }
    if (mpptData.loadCurrent_IL_mA.first != 0) {
        publishSensor("MPPT load current", NULL, "IL", "current", "measurement", "A", mpptData);
    }

    // optional info, provided only if TX is connected to charge controller
    if (mpptData.NetworkTotalDcInputPowerMilliWatts.first != 0) {
        publishSensor("VE.Smart network total DC input power", "mdi:solar-power", "NetworkTotalDcInputPower", "power", "measurement", "W", mpptData);
    }
    if (mpptData.MpptTemperatureMilliCelsius.first != 0) {
        publishSensor("MPPT temperature", "mdi:temperature-celsius", "MpptTemperature", "temperature", "measurement", "°C", mpptData);
    }
    if (mpptData.BatteryAbsorptionMilliVolt.first != 0) {
        publishSensor("Battery absorption voltage", "mdi:battery-charging-90", "BatteryAbsorption", "voltage", "measurement", "V", mpptData);
    }
    if (mpptData.BatteryFloatMilliVolt.first != 0) {
        publishSensor("Battery float voltage", "mdi:battery-charging-100", "BatteryFloat", "voltage", "measurement", "V", mpptData);
    }
    if (mpptData.SmartBatterySenseTemperatureMilliCelsius.first != 0) {
        publishSensor("Smart Battery Sense temperature", "mdi:temperature-celsius", "SmartBatterySenseTemperature", "temperature", "measurement", "°C", mpptData);
    }
}

void HassIntegration::publishSensor(const char *caption, const char *icon, const char *subTopic,
                                                const char *deviceClass, const char *stateClass,
                                                const char *unitOfMeasurement,
                                                const VeDirectMpptController::data_t &mpptData) const
{
    String serial = mpptData.serialNr_SER;

    String sensorId = caption;
    sensorId.replace(" ", "_");
    sensorId.replace(".", "");
    sensorId.replace("(", "");
    sensorId.replace(")", "");
    sensorId.toLowerCase();

    String configTopic = "sensor/dtu_victron_" + serial
        + "/" + sensorId
        + "/config";

    String statTopic = MqttSettings.getPrefix() + "victron/";
    statTopic.concat(serial);
    statTopic.concat("/");
    statTopic.concat(subTopic);

    JsonDocument root;

    root["name"] = caption;
    root["stat_t"] = statTopic;
    root["uniq_id"] = serial + "_" + sensorId;

    if (icon != NULL) {
        root["icon"] = icon;
    }

    if (unitOfMeasurement != NULL) {
        root["unit_of_meas"] = unitOfMeasurement;
    }

    JsonObject deviceObj = root["dev"].to<JsonObject>();
    createDeviceInfo(deviceObj, mpptData);

    if (Configuration.get().Mqtt.Hass.Expire) {
        root["exp_aft"] = Configuration.get().Mqtt.PublishInterval * 3;
    }
    if (deviceClass != NULL) {
        root["dev_cla"] = deviceClass;
    }
    if (stateClass != NULL) {
        root["stat_cla"] = stateClass;
    }

    if (!Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
        return;
    }

    char buffer[512];
    serializeJson(root, buffer);
    publish(configTopic, buffer);

}

void HassIntegration::publishBinarySensor(const char *caption, const char *icon, const char *subTopic,
                                                      const char *payload_on, const char *payload_off,
                                                      const VeDirectMpptController::data_t &mpptData) const
{
    String serial = mpptData.serialNr_SER;

    String sensorId = caption;
    sensorId.replace(" ", "_");
    sensorId.replace(".", "");
    sensorId.replace("(", "");
    sensorId.replace(")", "");
    sensorId.toLowerCase();

    String configTopic = "binary_sensor/dtu_victron_" + serial
        + "/" + sensorId
        + "/config";

    String statTopic = MqttSettings.getPrefix() + "victron/";
    statTopic.concat(serial);
    statTopic.concat("/");
    statTopic.concat(subTopic);

    JsonDocument root;
    root["name"] = caption;
    root["uniq_id"] = serial + "_" + sensorId;
    root["stat_t"] = statTopic;
    root["pl_on"] = payload_on;
    root["pl_off"] = payload_off;

    if (icon != NULL) {
        root["icon"] = icon;
    }

    JsonObject deviceObj = root["dev"].to<JsonObject>();
    createDeviceInfo(deviceObj, mpptData);

    if (!Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
        return;
    }

    char buffer[512];
    serializeJson(root, buffer);
    publish(configTopic, buffer);
}

void HassIntegration::createDeviceInfo(JsonObject &object,
                                                   const VeDirectMpptController::data_t &mpptData) const
{
    String serial = mpptData.serialNr_SER;
    object["name"] = "Victron(" + serial + ")";
    object["ids"] = serial;
    object["cu"] = MqttHandleHass.getDtuUrl();
    object["mf"] = "OpenDTU";
    object["mdl"] = mpptData.getPidAsString();
    object["sw"] = __COMPILED_GIT_HASH__;
    object["via_device"] = MqttHandleHass.getDtuUniqueId();
}

} // namespace SolarChargers::Victron
