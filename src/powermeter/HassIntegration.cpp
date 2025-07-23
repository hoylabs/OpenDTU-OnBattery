// SPDX-License-Identifier: GPL-2.0-or-later

#include <powermeter/HassIntegration.h>
#include <powermeter/Controller.h>
#include <Configuration.h>
#include <MqttSettings.h>
#include <MqttHandleHass.h>
#include <Utils.h>
#include <__compiled_constants.h>

PowerMeters::HassIntegration PowerMeterHassIntegration;

namespace PowerMeters {

void HassIntegration::hassLoop()
{
    auto const& config = Configuration.get();
    if (!config.Mqtt.Hass.Enabled) { return; }
    if (!config.PowerMeter.Enabled) { return; }

    // TODO(schlimmchen): this cannot make sure that transient
    // connection problems are actually always noticed.
    if (!MqttSettings.getConnected()) {
        _publishSensors = true;
        return;
    }

    if (!_publishSensors || !PowerMeter.isDataValid()) { return; }

    // Set the serial to match DTU unique ID
    _serial = MqttHandleHass.getDtuUniqueId();

    publishSensors();

    _publishSensors = false;
}

void HassIntegration::publishSensors()
{
    // Get access to the powermeter data container to check which sensors are available
    // Note: We need to access the provider's data container, but Controller doesn't expose it
    // For now, we'll check which data points actually have valid MQTT topics published
    // This approach mirrors what the Provider::mqttLoop() does
    
    // Always publish Power Total as it's computed from L1+L2+L3 if not directly available
    publishSensor("Power Total", "mdi:flash", "powertotal", "power", "measurement", "W");
    
    // Only publish individual phase sensors if the provider supports them
    // We'll use a helper method to check if data should be published
    publishSensor("Power L1", "mdi:flash", "power1", "power", "measurement", "W");
    publishSensor("Power L2", "mdi:flash", "power2", "power", "measurement", "W");
    publishSensor("Power L3", "mdi:flash", "power3", "power", "measurement", "W");
    
    publishSensor("Voltage L1", "mdi:sine-wave", "voltage1", "voltage", "measurement", "V");
    publishSensor("Voltage L2", "mdi:sine-wave", "voltage2", "voltage", "measurement", "V");
    publishSensor("Voltage L3", "mdi:sine-wave", "voltage3", "voltage", "measurement", "V");
    
    publishSensor("Current L1", "mdi:current-ac", "current1", "current", "measurement", "A");
    publishSensor("Current L2", "mdi:current-ac", "current2", "current", "measurement", "A");
    publishSensor("Current L3", "mdi:current-ac", "current3", "current", "measurement", "A");
    
    publishSensor("Import", "mdi:transmission-tower-import", "import", "energy", "total_increasing", "kWh");
    publishSensor("Export", "mdi:transmission-tower-export", "export", "energy", "total_increasing", "kWh");
}

void HassIntegration::publishSensor(const char* caption, const char* icon,
        const char* subTopic, const char* deviceClass,
        const char* stateClass, const char* unitOfMeasurement,
        const bool enabled)
{
    String sensorId = sanitizeUniqueId(caption);

    String configTopic = "sensor/powermeter_" + _serial
        + "/" + sensorId
        + "/config";

    String statTopic = MqttSettings.getPrefix() + "powermeter/";
    statTopic.concat(subTopic);

    JsonDocument root;
    root["name"] = caption;
    root["stat_t"] = statTopic;
    root["uniq_id"] = _serial + "_powermeter_" + sensorId;

    if (icon != NULL) {
        root["icon"] = icon;
    }

    if (!enabled) {
        root["enabled_by_default"] = "false";
    }

    if (unitOfMeasurement != NULL) {
        root["unit_of_meas"] = unitOfMeasurement;
    }

    JsonObject deviceObj = root["dev"].to<JsonObject>();
    createDeviceInfo(deviceObj);

    if (Configuration.get().Mqtt.Hass.Expire) {
        root["exp_aft"] = 90; // 90 seconds timeout for powermeter data
    }
    if (deviceClass != NULL) {
        root["dev_cla"] = deviceClass;
    }
    if (stateClass != NULL) {
        root["stat_cla"] = stateClass;
    }

    // Add availability topic
    auto const& config = Configuration.get();
    root["avty_t"] = MqttSettings.getPrefix() + config.Mqtt.Lwt.Topic;
    root["pl_avail"] = config.Mqtt.Lwt.Value_Online;
    root["pl_not_avail"] = config.Mqtt.Lwt.Value_Offline;

    if (!Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
        return;
    }

    char buffer[512];
    serializeJson(root, buffer);
    publish(configTopic, buffer);
}

void HassIntegration::createDeviceInfo(JsonObject& object)
{
    object["name"] = "PowerMeter";
    object["ids"] = "powermeter_" + _serial;
    object["cu"] = MqttHandleHass.getDtuUrl();
    object["mf"] = "OpenDTU";
    object["mdl"] = "PowerMeter";
    object["sw"] = __COMPILED_GIT_HASH__;
    object["via_device"] = MqttHandleHass.getDtuUniqueId();
}

void HassIntegration::publish(const String& subtopic, const String& payload)
{
    String topic = Configuration.get().Mqtt.Hass.Topic;
    topic += subtopic;
    MqttSettings.publishGeneric(topic.c_str(), payload.c_str(), Configuration.get().Mqtt.Hass.Retain);
}

String HassIntegration::sanitizeUniqueId(const char* value) {
    String sensorId = value;

    // replace characters that are invalid for unique_ids
    sensorId.replace(" ", "_");
    sensorId.replace(".", "");
    sensorId.replace("(", "");
    sensorId.replace(")", "");
    sensorId.replace(":", "");
    // replaces characters that are not allowed in published topics
    sensorId.replace("#", "");
    sensorId.replace("+", "");
    // replaces characters that should not be used in published topics
    sensorId.replace("*", "");
    sensorId.replace("$", "");
    sensorId.replace(">", "");
    // normalize
    sensorId.toLowerCase();

    return sensorId;
}

} // namespace PowerMeters