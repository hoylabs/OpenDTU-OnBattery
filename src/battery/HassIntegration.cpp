// SPDX-License-Identifier: GPL-2.0-or-later

#include <battery/Controller.h>
#include <battery/Stats.h>
#include <battery/HassIntegration.h>
#include <Configuration.h>
#include <MqttSettings.h>
#include <MqttHandleHass.h>
#include <Utils.h>
#include <__compiled_constants.h>
#include <LogHelper.h>

#undef TAG
static const char* TAG = "battery";
static const char* SUBTAG = "HASS";

namespace Batteries {

HassIntegration::HassIntegration(std::shared_ptr<Stats> spStats)
    : _spStats(spStats) { }

HassIntegration::~HassIntegration()
{
    stopLegacyCleanup();
}

void HassIntegration::hassLoop()
{
    auto const& config = Configuration.get();
    if (!config.Mqtt.Hass.Enabled) { return; }

    // TODO(schlimmchen): this cannot make sure that transient
    // connection problems are actually always noticed.
    if (!MqttSettings.getConnected()) {
        _publishSensors = true;
        stopLegacyCleanup();
        return;
    }

    if (!_publishSensors ||
        !_spStats->getManufacturer().has_value() ||
        !_spStats->getHassDeviceName().has_value()) { return; }

    if (!_oLegacyCleanupStart) {
        startLegacyCleanup();
        return;
    }

    clearLegacyConfigs();

    // give HASS time to delete the old entities first, otherwise the new
    // ones get an entity id with suffix "_2"
    if (millis() - *_oLegacyCleanupStart < 3000) { return; }

    stopLegacyCleanup();

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

void HassIntegration::publishSensor(const char* caption, const char* icon,
        const char* subTopic, const char* deviceClass,
        const char* stateClass, const char* unitOfMeasurement,
        const bool enabled) const
{
    String sensorId = sanitizeUniqueId(caption);

    if (_legacyCleanupOnly) {
        MqttSettings.publishGeneric(legacyConfigTopic("sensor", sensorId), "", true);
        return;
    }

    String configTopic = "sensor/" + createBatteryId()
        + "/" + sensorId
        + "/config";

    String statTopic = MqttSettings.getPrefix() + "battery/";
    statTopic.concat(subTopic);

    JsonDocument root;
    root["name"] = caption;
    root["stat_t"] = statTopic;
    root["uniq_id"] = createBatteryId() + "_" + sensorId;

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
        const char* payload_on, const char* payload_off,
        const bool enabled) const
{
    String sensorId = sanitizeUniqueId(caption);

    if (_legacyCleanupOnly) {
        MqttSettings.publishGeneric(legacyConfigTopic("binary_sensor", sensorId), "", true);
        return;
    }

    String configTopic = "binary_sensor/" + createBatteryId()
        + "/" + sensorId
        + "/config";

    String statTopic = MqttSettings.getPrefix() + "battery/";
    statTopic.concat(subTopic);

    JsonDocument root;

    root["name"] = caption;
    root["uniq_id"] = createBatteryId() + "_" + sensorId;
    root["stat_t"] = statTopic;
    root["pl_on"] = payload_on;
    root["pl_off"] = payload_off;

    if (icon != NULL) {
        root["icon"] = icon;
    }

    if (!enabled) {
        root["enabled_by_default"] = "false";
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
    object["ids"] = createBatteryId();
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

// All batteries used to share the hardcoded device id "0001", which made
// batteries of multiple DTUs collide in HASS. Clearing the old discovery
// configs makes HASS delete the old entities (and the old device) before the
// new ones are announced, so these get the old entity ids and keep their
// history. An empty message must be retained to remove a retained config
// from the broker, independent of the retain setting.
//
// This runs before every discovery: it is a no-op once the old configs are
// gone, and it also catches old configs reappearing (downgrade, a second DTU
// with older firmware). It can be removed again once upgrades from firmware
// using "0001" are no longer expected.
String HassIntegration::legacyConfigTopic(String const& component, String const& sensorId)
{
    return String(Configuration.get().Mqtt.Hass.Topic) + component + "/dtu_battery_0001/" + sensorId + "/config";
}

void HassIntegration::startLegacyCleanup()
{
    // finds all retained old configs, also those of sensors that are no longer
    // published (renamed sensors, a different battery provider before)
    MqttSettings.subscribe(legacyConfigTopic("+", "+"), 0,
        [this](espMqttClientTypes::MessageProperties const& properties,
                char const* topic, uint8_t const*, size_t len) {
            if (!properties.retain || len == 0) { return; }
            std::lock_guard<std::mutex> lock(_legacyTopicsMutex);
            _legacyTopics.emplace_back(topic);
        });

    // with retain disabled there is nothing on the broker to find, but HASS
    // still knows the old configs if it was running while they were announced
    _legacyCleanupOnly = true;
    publishSensors();
    _legacyCleanupOnly = false;

    _oLegacyCleanupStart = millis();
}

void HassIntegration::clearLegacyConfigs()
{
    std::vector<String> topics;
    {
        std::lock_guard<std::mutex> lock(_legacyTopicsMutex);
        topics.swap(_legacyTopics);
    }

    for (auto const& topic : topics) {
        DTU_LOGI("Removing legacy Home Assistant config %s", topic.c_str());
        MqttSettings.publishGeneric(topic, "", true);
    }
}

void HassIntegration::stopLegacyCleanup()
{
    if (!_oLegacyCleanupStart) { return; }

    MqttSettings.unsubscribe(legacyConfigTopic("+", "+"));
    clearLegacyConfigs();
    _oLegacyCleanupStart.reset();
}

// The battery state topics (<prefix>battery/...) are only distinguished by
// the MQTT prefix, so it has to be unique per DTU on a broker anyway. Unlike
// the hostname or the chip id, it survives renaming the DTU and replacing the
// ESP32 (the user sets the same prefix again, other MQTT consumers depend on
// it). HASS only allows [a-zA-Z0-9_-] in the discovery topic node id, other
// characters are escaped as "_<hex>", so distinct prefixes never map to the
// same id (e.g. "dtu/a/" and "dtu_a/"). "solar/" -> "dtu_battery_solar".
String HassIntegration::createBatteryId() {
    String prefix = MqttSettings.getPrefix();
    if (prefix.endsWith("/")) { prefix.remove(prefix.length() - 1); }

    String id = "dtu_battery_";
    for (char c : prefix) {
        auto const b = static_cast<unsigned char>(c);
        if (isalnum(b) || c == '-') { id += c; continue; }

        char escaped[4];
        snprintf(escaped, sizeof(escaped), "_%02x", b);
        id += escaped;
    }
    return id;
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

} // namespace Batteries
