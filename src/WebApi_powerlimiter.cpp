// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "WebApi_powerlimiter.h"
#include "VeDirectFrameHandler.h"
#include "ArduinoJson.h"
#include "AsyncJson.h"
#include "Configuration.h"
#include "WebApi.h"
#include "helper.h"

void WebApiPowerLimiterClass::init(AsyncWebServer* server)
{
    using std::placeholders::_1;

    _server = server;

    _server->on("/api/powerlimiter/status", HTTP_GET, std::bind(&WebApiPowerLimiterClass::onStatus, this, _1));
    _server->on("/api/powerlimiter/config", HTTP_GET, std::bind(&WebApiPowerLimiterClass::onAdminGet, this, _1));
    _server->on("/api/powerlimiter/config", HTTP_POST, std::bind(&WebApiPowerLimiterClass::onAdminPost, this, _1));
}

void WebApiPowerLimiterClass::loop()
{
}

void WebApiPowerLimiterClass::onStatus(AsyncWebServerRequest* request)
{
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();
    const CONFIG_T& config = Configuration.get();

    root[F("enabled")] = config.PowerLimiter_Enabled;
    root[F("mqtt_topic_powermeter_1")] = config.PowerLimiter_MqttTopicPowerMeter1;
    root[F("mqtt_topic_powermeter_2")] = config.PowerLimiter_MqttTopicPowerMeter2;
    root[F("mqtt_topic_powermeter_3")] = config.PowerLimiter_MqttTopicPowerMeter3;

    response->setLength();
    request->send(response);
}

void WebApiPowerLimiterClass::onAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();
    const CONFIG_T& config = Configuration.get();

    root[F("enabled")] = config.PowerLimiter_Enabled;
    root[F("mqtt_topic_powermeter_1")] = config.PowerLimiter_MqttTopicPowerMeter1;
    root[F("mqtt_topic_powermeter_2")] = config.PowerLimiter_MqttTopicPowerMeter2;
    root[F("mqtt_topic_powermeter_3")] = config.PowerLimiter_MqttTopicPowerMeter3;

    response->setLength();
    request->send(response);
}

void WebApiPowerLimiterClass::onAdminPost(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject retMsg = response->getRoot();
    retMsg[F("type")] = F("warning");

    if (!request->hasParam("data", true)) {
        retMsg[F("message")] = F("No values found!");
        response->setLength();
        request->send(response);
        return;
    }

    String json = request->getParam("data", true)->value();

    if (json.length() > 1024) {
        retMsg[F("message")] = F("Data too large!");
        response->setLength();
        request->send(response);
        return;
    }

    DynamicJsonDocument root(1024);
    DeserializationError error = deserializeJson(root, json);

    if (error) {
        retMsg[F("message")] = F("Failed to parse data!");
        response->setLength();
        request->send(response);
        return;
    }

    if (!(root.containsKey("enabled") && root.containsKey("mqtt_topic_powermeter_1") && root.containsKey("mqtt_topic_powermeter_2")
            && root.containsKey("mqtt_topic_powermeter_3"))) {
        retMsg[F("message")] = F("Values are missing!");
        response->setLength();
        request->send(response);
        return;
    }

    CONFIG_T& config = Configuration.get();
    config.PowerLimiter_Enabled = root[F("enabled")].as<bool>();
    strlcpy(config.PowerLimiter_MqttTopicPowerMeter1, root[F("mqtt_topic_powermeter_1")].as<String>().c_str(), sizeof(config.PowerLimiter_MqttTopicPowerMeter1));
    strlcpy(config.PowerLimiter_MqttTopicPowerMeter2, root[F("mqtt_topic_powermeter_2")].as<String>().c_str(), sizeof(config.PowerLimiter_MqttTopicPowerMeter2));
    strlcpy(config.PowerLimiter_MqttTopicPowerMeter3, root[F("mqtt_topic_powermeter_3")].as<String>().c_str(), sizeof(config.PowerLimiter_MqttTopicPowerMeter3));
    Configuration.write();

    retMsg[F("type")] = F("success");
    retMsg[F("message")] = F("Settings saved!");

    response->setLength();
    request->send(response);
}
