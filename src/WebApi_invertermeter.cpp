// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#include "WebApi_invertermeter.h"
#include "ArduinoJson.h"
#include "AsyncJson.h"
#include "Configuration.h"
#include "MqttHandleHass.h"
#include "MqttSettings.h"
#include "PowerLimiter.h"
#include <invertermeter/Controller.h>
#include <powermeter/json/http/Provider.h>
#include <powermeter/sml/http/Provider.h>
#include "WebApi.h"
#include "helper.h"

void WebApiInverterMeterClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    _server = &server;

    _server->on("/api/invertermeter/status", HTTP_GET, std::bind(&WebApiInverterMeterClass::onStatus, this, _1));
    _server->on("/api/invertermeter/config", HTTP_GET, std::bind(&WebApiInverterMeterClass::onAdminGet, this, _1));
    _server->on("/api/invertermeter/config", HTTP_POST, std::bind(&WebApiInverterMeterClass::onAdminPost, this, _1));
    _server->on("/api/invertermeter/testhttpjsonrequest", HTTP_POST, std::bind(&WebApiInverterMeterClass::onTestHttpJsonRequest, this, _1));
    _server->on("/api/invertermeter/testhttpsmlrequest", HTTP_POST, std::bind(&WebApiInverterMeterClass::onTestHttpSmlRequest, this, _1));
}

void WebApiInverterMeterClass::onStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    auto const& config = Configuration.get();

    root["enabled"] = config.InverterMeter.Enabled;
    root["source"] = config.InverterMeter.Source;

    auto mqtt = root["mqtt"].to<JsonObject>();
    Configuration.serializePowerMeterMqttConfig(config.InverterMeter.Mqtt, mqtt);

    auto serialSdm = root["serial_sdm"].to<JsonObject>();
    Configuration.serializePowerMeterSerialSdmConfig(config.InverterMeter.SerialSdm, serialSdm);

    auto httpJson = root["http_json"].to<JsonObject>();
    Configuration.serializePowerMeterHttpJsonConfig(config.InverterMeter.HttpJson, httpJson);

    auto httpSml = root["http_sml"].to<JsonObject>();
    Configuration.serializePowerMeterHttpSmlConfig(config.InverterMeter.HttpSml, httpSml);

    auto udpVictron = root["udp_victron"].to<JsonObject>();
    Configuration.serializePowerMeterUdpVictronConfig(config.InverterMeter.UdpVictron, udpVictron);

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiInverterMeterClass::onAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    this->onStatus(request);
}

void WebApiInverterMeterClass::onAdminPost(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonDocument root;
    if (!WebApi.parseRequestData(request, response, root)) {
        return;
    }

    auto& retMsg = response->getRoot();

    if (!(root["enabled"].is<bool>() && root["source"].is<uint32_t>())) {
        retMsg["message"] = "Values are missing!";
        response->setLength();
        request->send(response);
        return;
    }

    auto checkHttpConfig = [&](JsonObject const& cfg) -> bool {
        if (!cfg["url"].is<String>()
                || (!cfg["url"].as<String>().startsWith("http://")
                    && !cfg["url"].as<String>().startsWith("https://"))) {
            retMsg["message"] = "URL must either start with http:// or https://!";
            response->setLength();
            request->send(response);
            return false;
        }

        if (cfg["auth_type"].is<uint8_t>()) {
            auto authType = cfg["auth_type"].as<uint8_t>();
            if (authType > static_cast<uint8_t>(HttpRequestConfig::Auth::Digest)) {
                retMsg["message"] = "Unknown authentication type!";
                response->setLength();
                request->send(response);
                return false;
            }
        }

        return true;
    };

    {
        CONFIG_T& config = Configuration.getWriteGuard().getConfig();

        config.InverterMeter.Enabled = root["enabled"].as<bool>();
        config.InverterMeter.Source = root["source"].as<uint32_t>();

        if (root["mqtt"].is<JsonObject>()) {
            Configuration.deserializePowerMeterMqttConfig(root["mqtt"], config.InverterMeter.Mqtt);
        }

        if (root["serial_sdm"].is<JsonObject>()) {
            Configuration.deserializePowerMeterSerialSdmConfig(root["serial_sdm"], config.InverterMeter.SerialSdm);
        }

        if (root["http_json"].is<JsonObject>()) {
            if (!checkHttpConfig(root["http_json"])) { return; }
            Configuration.deserializePowerMeterHttpJsonConfig(root["http_json"], config.InverterMeter.HttpJson);
        }

        if (root["http_sml"].is<JsonObject>()) {
            if (!checkHttpConfig(root["http_sml"])) { return; }
            Configuration.deserializePowerMeterHttpSmlConfig(root["http_sml"], config.InverterMeter.HttpSml);
        }

        if (root["udp_victron"].is<JsonObject>()) {
            Configuration.deserializePowerMeterUdpVictronConfig(root["udp_victron"], config.InverterMeter.UdpVictron);
        }
    }

    WebApi.writeConfig(retMsg);

    InverterMeter.updateSettings();

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiInverterMeterClass::onTestHttpJsonRequest(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonDocument root;
    if (!WebApi.parseRequestData(request, response, root)) {
        return;
    }

    auto& retMsg = response->getRoot();

    PowerMeterHttpJsonConfig test_config;
    Configuration.deserializePowerMeterHttpJsonConfig(root, test_config);

    auto provider = ::PowerMeters::Json::Http::Provider(test_config);
    if (!provider.init()) {
        retMsg["message"] = "Invalid settings!";
        response->setLength();
        request->send(response);
        return;
    }

    auto response_body = provider.getHttpPowerMeterResponseBody();
    retMsg["message"] = "HTTP response received!";
    retMsg["value"] = response_body;

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiInverterMeterClass::onTestHttpSmlRequest(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonDocument root;
    if (!WebApi.parseRequestData(request, response, root)) {
        return;
    }

    auto& retMsg = response->getRoot();

    PowerMeterHttpSmlConfig test_config;
    Configuration.deserializePowerMeterHttpSmlConfig(root, test_config);

    auto provider = ::PowerMeters::Sml::Http::Provider(test_config);
    if (!provider.init()) {
        retMsg["message"] = "Invalid settings!";
        response->setLength();
        request->send(response);
        return;
    }

    auto response_body = provider.getHttpPowerMeterResponseBody();
    retMsg["message"] = "HTTP response received!";
    retMsg["value"] = response_body;

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}