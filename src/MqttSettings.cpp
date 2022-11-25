// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "MqttSettings.h"
#include "MessageOutput.h"
#include "Configuration.h"
#include "NetworkSettings.h"
#include "ZeroExportPowerLimiter.h"
#include <Hoymiles.h>
#include <MqttClientSetup.h>
#include <Ticker.h>
#include <espMqttClient.h>

#define TOPIC_SUB_LIMIT_PERSISTENT_RELATIVE "limit_persistent_relative"
#define TOPIC_SUB_LIMIT_PERSISTENT_ABSOLUTE "limit_persistent_absolute"
#define TOPIC_SUB_LIMIT_NONPERSISTENT_RELATIVE "limit_nonpersistent_relative"
#define TOPIC_SUB_LIMIT_NONPERSISTENT_ABSOLUTE "limit_nonpersistent_absolute"
#define TOPIC_SUB_POWER "power"
#define TOPIC_SUB_RESTART "restart"

MqttSettingsClass::MqttSettingsClass()
{
}

void MqttSettingsClass::NetworkEvent(network_event event)
{
    switch (event) {
    case network_event::NETWORK_GOT_IP:
        MessageOutput.println(F("Network connected"));
        performConnect();
        break;
    case network_event::NETWORK_DISCONNECTED:
        MessageOutput.println(F("Network lost connection"));
        mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
        break;
    default:
        break;
    }
}

void MqttSettingsClass::onMqttConnect(bool sessionPresent)
{
    MessageOutput.println(F("Connected to MQTT."));
    const CONFIG_T& config = Configuration.get();
    publish(config.Mqtt_LwtTopic, config.Mqtt_LwtValue_Online);

    for (const auto& cb : _mqttSubscribeParser.get_callbacks()) {
        mqttClient->subscribe(cb.topic.c_str(), cb.qos);
    }

    String topic = getPrefix();
    mqttClient->subscribe(String(topic + "+/cmd/" + TOPIC_SUB_LIMIT_PERSISTENT_RELATIVE).c_str(), 0);
    mqttClient->subscribe(String(topic + "+/cmd/" + TOPIC_SUB_LIMIT_PERSISTENT_ABSOLUTE).c_str(), 0);
    mqttClient->subscribe(String(topic + "+/cmd/" + TOPIC_SUB_LIMIT_NONPERSISTENT_RELATIVE).c_str(), 0);
    mqttClient->subscribe(String(topic + "+/cmd/" + TOPIC_SUB_LIMIT_NONPERSISTENT_ABSOLUTE).c_str(), 0);
    mqttClient->subscribe(String(topic + "+/cmd/" + TOPIC_SUB_POWER).c_str(), 0);
    mqttClient->subscribe(String(topic + "+/cmd/" + TOPIC_SUB_RESTART).c_str(), 0);

    // Zero export power limiter
    if (strlen(TOPIC_CURRENT_POWER_CONSUMPTION_1) != 0) {
        mqttClient->subscribe(TOPIC_CURRENT_POWER_CONSUMPTION_1, 0);
    }

    if (strlen(TOPIC_CURRENT_POWER_CONSUMPTION_2) != 0) {
        mqttClient->subscribe(TOPIC_CURRENT_POWER_CONSUMPTION_2, 0);
    }

    if (strlen(TOPIC_CURRENT_POWER_CONSUMPTION_3) != 0) {
        mqttClient->subscribe(TOPIC_CURRENT_POWER_CONSUMPTION_3, 0);
    }
}

void MqttSettingsClass::subscribe(const String& topic, uint8_t qos, const espMqttClientTypes::OnMessageCallback& cb)
{
    _mqttSubscribeParser.register_callback(topic.c_str(), qos, cb);
    mqttClient->subscribe(topic.c_str(), qos);
}

void MqttSettingsClass::unsubscribe(const String& topic)
{
    _mqttSubscribeParser.unregister_callback(topic.c_str());
    mqttClient->unsubscribe(topic.c_str());
}

void MqttSettingsClass::onMqttDisconnect(espMqttClientTypes::DisconnectReason reason)
{
    MessageOutput.println(F("Disconnected from MQTT."));

    MessageOutput.print(F("Disconnect reason:"));
    switch (reason) {
    case espMqttClientTypes::DisconnectReason::TCP_DISCONNECTED:
        MessageOutput.println(F("TCP_DISCONNECTED"));
        break;
    case espMqttClientTypes::DisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
        MessageOutput.println(F("MQTT_UNACCEPTABLE_PROTOCOL_VERSION"));
        break;
    case espMqttClientTypes::DisconnectReason::MQTT_IDENTIFIER_REJECTED:
        MessageOutput.println(F("MQTT_IDENTIFIER_REJECTED"));
        break;
    case espMqttClientTypes::DisconnectReason::MQTT_SERVER_UNAVAILABLE:
        MessageOutput.println(F("MQTT_SERVER_UNAVAILABLE"));
        break;
    case espMqttClientTypes::DisconnectReason::MQTT_MALFORMED_CREDENTIALS:
        MessageOutput.println(F("MQTT_MALFORMED_CREDENTIALS"));
        break;
    case espMqttClientTypes::DisconnectReason::MQTT_NOT_AUTHORIZED:
        MessageOutput.println(F("MQTT_NOT_AUTHORIZED"));
        break;
    default:
        MessageOutput.println(F("Unknown"));
    }
    mqttReconnectTimer.once(
        2, +[](MqttSettingsClass* instance) { instance->performConnect(); }, this);
}

void MqttSettingsClass::onMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total)
{
    MessageOutput.print(F("Received MQTT message on topic: "));
    MessageOutput.println(topic);

    if (strcmp(topic, TOPIC_CURRENT_POWER_CONSUMPTION_1) == 0
            || strcmp(topic, TOPIC_CURRENT_POWER_CONSUMPTION_2) == 0
            || strcmp(topic, TOPIC_CURRENT_POWER_CONSUMPTION_3) == 0) {
        ZeroExportPowerLimiter.onMqttMessage(properties, topic, payload, len, index, total);
        return;
    }

    _mqttSubscribeParser.handle_message(properties, topic, payload, len, index, total);
}

void MqttSettingsClass::performConnect()
{
    if (NetworkSettings.isConnected() && Configuration.get().Mqtt_Enabled) {
        using std::placeholders::_1;
        using std::placeholders::_2;
        using std::placeholders::_3;
        using std::placeholders::_4;
        using std::placeholders::_5;
        using std::placeholders::_6;
        MessageOutput.println(F("Connecting to MQTT..."));
        const CONFIG_T& config = Configuration.get();
        willTopic = getPrefix() + config.Mqtt_LwtTopic;
        clientId = NetworkSettings.getApName();
        if (config.Mqtt_Tls) {
            static_cast<espMqttClientSecure*>(mqttClient)->setCACert(config.Mqtt_RootCaCert);
            static_cast<espMqttClientSecure*>(mqttClient)->setServer(config.Mqtt_Hostname, config.Mqtt_Port);
            static_cast<espMqttClientSecure*>(mqttClient)->setCredentials(config.Mqtt_Username, config.Mqtt_Password);
            static_cast<espMqttClientSecure*>(mqttClient)->setWill(willTopic.c_str(), 2, config.Mqtt_Retain, config.Mqtt_LwtValue_Offline);
            static_cast<espMqttClientSecure*>(mqttClient)->setClientId(clientId.c_str());
            static_cast<espMqttClientSecure*>(mqttClient)->onConnect(std::bind(&MqttSettingsClass::onMqttConnect, this, _1));
            static_cast<espMqttClientSecure*>(mqttClient)->onDisconnect(std::bind(&MqttSettingsClass::onMqttDisconnect, this, _1));
            static_cast<espMqttClientSecure*>(mqttClient)->onMessage(std::bind(&MqttSettingsClass::onMqttMessage, this, _1, _2, _3, _4, _5, _6));
        } else {
            static_cast<espMqttClient*>(mqttClient)->setServer(config.Mqtt_Hostname, config.Mqtt_Port);
            static_cast<espMqttClient*>(mqttClient)->setCredentials(config.Mqtt_Username, config.Mqtt_Password);
            static_cast<espMqttClient*>(mqttClient)->setWill(willTopic.c_str(), 2, config.Mqtt_Retain, config.Mqtt_LwtValue_Offline);
            static_cast<espMqttClient*>(mqttClient)->setClientId(clientId.c_str());
            static_cast<espMqttClient*>(mqttClient)->onConnect(std::bind(&MqttSettingsClass::onMqttConnect, this, _1));
            static_cast<espMqttClient*>(mqttClient)->onDisconnect(std::bind(&MqttSettingsClass::onMqttDisconnect, this, _1));
            static_cast<espMqttClient*>(mqttClient)->onMessage(std::bind(&MqttSettingsClass::onMqttMessage, this, _1, _2, _3, _4, _5, _6));
        }
        mqttClient->connect();
    }
}

void MqttSettingsClass::performDisconnect()
{
    const CONFIG_T& config = Configuration.get();
    publish(config.Mqtt_LwtTopic, config.Mqtt_LwtValue_Offline);
    mqttClient->disconnect();
}

void MqttSettingsClass::performReconnect()
{
    performDisconnect();

    createMqttClientObject();

    mqttReconnectTimer.once(
        2, +[](MqttSettingsClass* instance) { instance->performConnect(); }, this);
}

bool MqttSettingsClass::getConnected()
{
    return mqttClient->connected();
}

String MqttSettingsClass::getPrefix()
{
    return Configuration.get().Mqtt_Topic;
}

void MqttSettingsClass::publish(const String& subtopic, const String& payload)
{
    String topic = getPrefix();
    topic += subtopic;
    mqttClient->publish(topic.c_str(), 0, Configuration.get().Mqtt_Retain, payload.c_str());
}

void MqttSettingsClass::publishGeneric(const String& topic, const String& payload, bool retain, uint8_t qos)
{
    mqttClient->publish(topic.c_str(), qos, retain, payload.c_str());
}

void MqttSettingsClass::init()
{
    using std::placeholders::_1;
    NetworkSettings.onEvent(std::bind(&MqttSettingsClass::NetworkEvent, this, _1));

    createMqttClientObject();
}

void MqttSettingsClass::createMqttClientObject()
{
    if (mqttClient != nullptr)
        delete mqttClient;
    const CONFIG_T& config = Configuration.get();
    if (config.Mqtt_Tls) {
        mqttClient = static_cast<MqttClient*>(new espMqttClientSecure);
    } else {
        mqttClient = static_cast<MqttClient*>(new espMqttClient);
    }
}

MqttSettingsClass MqttSettings;
