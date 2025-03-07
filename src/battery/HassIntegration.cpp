// SPDX-License-Identifier: GPL-2.0-or-later

#include <battery/Controller.h>
#include <battery/Stats.h>
#include <battery/HassIntegration.h>
#include <Configuration.h>
#include <MqttSettings.h>
#include <MqttHandleHass.h>
#include <Utils.h>
#include <__compiled_constants.h>

namespace Batteries {

HassIntegration::HassIntegration(std::shared_ptr<Stats> spStats)
    : _spStats(spStats) { }

void HassIntegration::hassLoop()
{
    auto const& config = Configuration.get();
    if (!config.Mqtt.Hass.Enabled) { return; }

    // TODO(schlimmchen): this cannot make sure that transient
    // connection problems are actually always noticed.
    if (!MqttSettings.getConnected()) {
        _publishSensors = true;
        return;
    }

    if (!_publishSensors ||
        !_spStats->getManufacturer().has_value() ||
        !_spStats->getHassDeviceName().has_value()) { return; }

    publishSensors();

    _publishSensors = false;
}

void HassIntegration::publishSensors() const
{
    publishSensor("Manufacturer",          "mdi:factory",        "manufacturer");
    publishSensor("Data Age",              "mdi:timer-sand",     "dataAge",       "duration", "measurement", "s");
    publishSensor("State of Charge (SoC)", "mdi:battery-medium", "stateOfCharge", "battery",  "measurement", "%");
    publishSensor("Voltage", "mdi:battery-charging", "voltage", "voltage", "measurement", "V");
    publishSensor("Current", "mdi:current-dc", "current", "current", "measurement", "A");
}

void HassIntegration::publishSensor(const String& caption, const char* icon,
        const String& subTopic, const char* deviceClass,
        const char* stateClass, const char* unitOfMeasurement) const
{
    publishSensor(caption.c_str(), icon, subTopic.c_str(), deviceClass, stateClass, unitOfMeasurement);
}

void HassIntegration::publishSensor(const char* caption, const char* icon,
        const String& subTopic, const char* deviceClass,
        const char* stateClass, const char* unitOfMeasurement) const
{
    publishSensor(caption, icon, subTopic.c_str(), deviceClass, stateClass, unitOfMeasurement);
}

void HassIntegration::publishSensor(const String& caption, const char* icon,
        const char* subTopic, const char* deviceClass,
        const char* stateClass, const char* unitOfMeasurement) const
{
    publishSensor(caption.c_str(), icon, subTopic, deviceClass, stateClass, unitOfMeasurement);
}

void HassIntegration::publishSensor(const char* caption, const char* icon,
        const char* subTopic, const char* deviceClass,
        const char* stateClass, const char* unitOfMeasurement) const
{
    String sensorId = caption;
    sensorId.replace(" ", "_");
    sensorId.replace(".", "");
    sensorId.replace("(", "");
    sensorId.replace(")", "");
    sensorId.toLowerCase();

    String configTopic = "sensor/dtu_battery_" + _serial
        + "/" + sensorId
        + "/config";

    String statTopic = MqttSettings.getPrefix() + "battery/";
    // omit serial to avoid a breaking change
    // statTopic.concat(_serial);
    // statTopic.concat("/");
    statTopic.concat(subTopic);

    JsonDocument root;
    root["name"] = caption;
    root["stat_t"] = statTopic;
    root["uniq_id"] = _serial + "_" + sensorId;

    if (icon != NULL) {
        root["icon"] = icon;
    }

    if (unitOfMeasurement != NULL) {
        root["unit_of_meas"] = unitOfMeasurement;
    }

    JsonObject deviceObj = root["dev"].to<JsonObject>();
    createDeviceInfo(deviceObj);

    if (Configuration.get().Mqtt.Hass.Expire) {
        root["exp_aft"] = _spStats->getMqttFullPublishIntervalMs() / 1000 * 3;
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

void HassIntegration::publishBinarySensor(const char* caption,
        const char* icon, const char* subTopic,
        const char* payload_on, const char* payload_off) const
{
    String sensorId = caption;
    sensorId.replace(" ", "_");
    sensorId.replace(".", "");
    sensorId.replace("(", "");
    sensorId.replace(")", "");
    sensorId.replace(":", "");
    sensorId.toLowerCase();

    String configTopic = "binary_sensor/dtu_battery_" + _serial
        + "/" + sensorId
        + "/config";

    String statTopic = MqttSettings.getPrefix() + "battery/";
    // omit serial to avoid a breaking change
    // statTopic.concat(_serial);
    // statTopic.concat("/");
    statTopic.concat(subTopic);

    JsonDocument root;

    root["name"] = caption;
    root["uniq_id"] = _serial + "_" + sensorId;
    root["stat_t"] = statTopic;
    root["pl_on"] = payload_on;
    root["pl_off"] = payload_off;

    if (icon != NULL) {
        root["icon"] = icon;
    }

    auto deviceObj = root["dev"].to<JsonObject>();
    createDeviceInfo(deviceObj);

    if (!Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
        return;
    }

    char buffer[512];
    serializeJson(root, buffer);
    publish(configTopic, buffer);
}

void HassIntegration::createDeviceInfo(JsonObject& object) const
{
    object["name"] = *_spStats->getHassDeviceName();
    object["ids"] = _serial;
    object["cu"] = MqttHandleHass.getDtuUrl();
    object["mf"] = "OpenDTU";
    object["mdl"] = *_spStats->getManufacturer();
    object["sw"] = __COMPILED_GIT_HASH__;
    object["via_device"] = MqttHandleHass.getDtuUniqueId();
}

void HassIntegration::publish(const String& subtopic, const String& payload) const
{
    String topic = Configuration.get().Mqtt.Hass.Topic;
    topic += subtopic;
    MqttSettings.publishGeneric(topic.c_str(), payload.c_str(), Configuration.get().Mqtt.Hass.Retain);
}

} // namespace Batteries
