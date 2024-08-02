// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2024 Thomas Basler and others
 */
#include "MqttHassPublisher.h"
#include "MqttSettings.h"
#include "NetworkSettings.h"
#include "Utils.h"
#include "Configuration.h"

void MqttHassPublisher::publish(const String& subtopic, const JsonDocument& payload)
{
    String buffer;
    serializeJson(payload, buffer);

    String topic = Configuration.get().Mqtt.Hass.Topic;
    topic += subtopic;
    MqttSettings.publishGeneric(topic, buffer, Configuration.get().Mqtt.Hass.Retain);
}


JsonObject MqttHassPublisher::createDeviceInfo(
    const String& name, const String& identifiers,
    const String& model, const String& sw_version,
    const bool& via_dtu)
{
    JsonObject object;

    object["name"] = name;
    object["ids"] = identifiers;
    object["cu"] = getDtuUrl(),
    object["mf"] = "OpenDTU";
    object["mdl"] = model;
    object["sw"] = sw_version;

    if (via_dtu) {
        object["via_device"] = getDtuUniqueId();
    }

    return object;
}

void MqttHassPublisher::publishBinarySensor(
    const String& unique_dentifier,
    const String& name, const String& icon,
    const String& sensorId, const String& statSubTopic)
{
    JsonDocument root;

    root["name"] = name;
    root["uniq_id"] = unique_dentifier;
    root["stat_t"] = MqttSettings.getPrefix() + "/" + statSubTopic;
    root["pl_on"] = "1";
    root["pl_off"] = "0";

    if (icon != nullptr) {
        root["icon"] = icon;
    }

    root["dev"] = createDeviceInfo();

    if (!Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
        return;
    }

    publish(
        "binary_sensor/" + configTopicPrefix() + "/" + sensorId + "/config",
        root
    );
}

String MqttHassPublisher::getDtuUniqueId()
{
    return NetworkSettings.getHostname() + "_" + Utils::getChipId();
}

String MqttHassPublisher::getDtuUrl()
{
    return String("http://") + NetworkSettings.localIP().toString();
}
