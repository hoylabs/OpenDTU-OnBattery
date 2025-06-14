// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#include "WebApi_powermeter.h"
#include "ArduinoJson.h"
#include "AsyncJson.h"
#include "Configuration.h"
#include "MqttHandleHass.h"
#include "MqttSettings.h"
#include "PowerLimiter.h"
#include <powermeter/Controller.h>
#include <powermeter/json/http/Provider.h>
#include <powermeter/sml/http/Provider.h>
#include "WebApi.h"
#include "helper.h"

void WebApiPowerMeterClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    _server = &server;

    _server->on("/api/powermeter/status", HTTP_GET, std::bind(&WebApiPowerMeterClass::onStatus, this, _1));
    _server->on("/api/powermeter/config", HTTP_GET, std::bind(&WebApiPowerMeterClass::onAdminGet, this, _1));
    _server->on("/api/powermeter/config", HTTP_POST, std::bind(&WebApiPowerMeterClass::onAdminPost, this, _1));
    _server->on("/api/powermeter/testhttpjsonrequest", HTTP_POST, std::bind(&WebApiPowerMeterClass::onTestHttpJsonRequest, this, _1));
    _server->on("/api/powermeter/testhttpsmlrequest", HTTP_POST, std::bind(&WebApiPowerMeterClass::onTestHttpSmlRequest, this, _1));
}

void WebApiPowerMeterClass::onStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    auto const& config = Configuration.get();

    root["enabled"] = config.PowerMeter.Enabled;
    root["source"] = config.PowerMeter.Source;

    auto mqtt = root["mqtt"].to<JsonObject>();
    Configuration.serializePowerMeterMqttConfig(config.PowerMeter.Mqtt, mqtt);

    auto serialSdm = root["serial_sdm"].to<JsonObject>();
    Configuration.serializePowerMeterSerialSdmConfig(config.PowerMeter.SerialSdm, serialSdm);

    auto httpJson = root["http_json"].to<JsonObject>();
    Configuration.serializePowerMeterHttpJsonConfig(config.PowerMeter.HttpJson, httpJson);

    auto httpSml = root["http_sml"].to<JsonObject>();
    Configuration.serializePowerMeterHttpSmlConfig(config.PowerMeter.HttpSml, httpSml);

    auto udpVictron = root["udp_victron"].to<JsonObject>();
    Configuration.serializePowerMeterUdpVictronConfig(config.PowerMeter.UdpVictron, udpVictron);

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiPowerMeterClass::onAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    this->onStatus(request);
}

void WebApiPowerMeterClass::onAdminPost(AsyncWebServerRequest* request)
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

        if ((cfg["auth_type"].as<uint8_t>() != HttpRequestConfig::Auth::None)
                && (cfg["username"].as<String>().length() == 0 || cfg["password"].as<String>().length() == 0)) {
            retMsg["message"] = "Username or password must not be empty!";
            response->setLength();
            request->send(response);
            return false;
        }

        if (!cfg["timeout"].is<uint16_t>()
                || cfg["timeout"].as<uint16_t>() <= 0) {
            retMsg["message"] = "Timeout must be greater than 0 ms!";
            response->setLength();
            request->send(response);
            return false;
        }

        return true;
    };

    if (static_cast<::PowerMeters::Provider::Type>(root["source"].as<uint8_t>()) == ::PowerMeters::Provider::Type::HTTP_JSON) {
        JsonObject httpJson = root["http_json"];
        JsonArray valueConfigs = httpJson["values"];
        for (uint8_t i = 0; i < valueConfigs.size(); i++) {
            JsonObject valueConfig = valueConfigs[i].as<JsonObject>();

            if (i > 0 && !valueConfig["enabled"].as<bool>()) {
                continue;
            }

            if (i == 0 || httpJson["individual_requests"].as<bool>()) {
                if (!checkHttpConfig(valueConfig["http_request"].as<JsonObject>())) {
                    return;
                }
            }

            if (!valueConfig["json_path"].is<String>()
                    || valueConfig["json_path"].as<String>().length() == 0) {
                retMsg["message"] = "Json path must not be empty!";
                response->setLength();
                request->send(response);
                return;
            }
        }
    }

    if (static_cast<::PowerMeters::Provider::Type>(root["source"].as<uint8_t>()) == ::PowerMeters::Provider::Type::HTTP_SML) {
        JsonObject httpSml = root["http_sml"];
        if (!checkHttpConfig(httpSml["http_request"].as<JsonObject>())) {
            return;
        }
    }

    if (static_cast<::PowerMeters::Provider::Type>(root["source"].as<uint8_t>()) == ::PowerMeters::Provider::Type::UDP_VICTRON) {
        JsonObject udpVictron = root["udp_victron"];
        if (!udpVictron["ip_address"].is<String>()
                || udpVictron["ip_address"].as<String>().length() == 0) {
            retMsg["message"] = "IP address must not be empty!";
            response->setLength();
            request->send(response);
            return;
        }

        if (!udpVictron["polling_interval_ms"].is<uint32_t>()
                || udpVictron["polling_interval_ms"].as<uint32_t>() <= 0) {
            retMsg["message"] = "Polling interval must be greater than 0 ms!";
            response->setLength();
            request->send(response);
            return;
        }
    }

    {
        auto guard = Configuration.getWriteGuard();
        auto& config = guard.getConfig();
        config.PowerMeter.Enabled = root["enabled"].as<bool>();
        config.PowerMeter.Source = root["source"].as<uint8_t>();

        Configuration.deserializePowerMeterMqttConfig(root["mqtt"].as<JsonObject>(),
                config.PowerMeter.Mqtt);

        Configuration.deserializePowerMeterSerialSdmConfig(root["serial_sdm"].as<JsonObject>(),
                config.PowerMeter.SerialSdm);

        Configuration.deserializePowerMeterHttpJsonConfig(root["http_json"].as<JsonObject>(),
                config.PowerMeter.HttpJson);

        Configuration.deserializePowerMeterHttpSmlConfig(root["http_sml"].as<JsonObject>(),
                config.PowerMeter.HttpSml);

        Configuration.deserializePowerMeterUdpVictronConfig(root["udp_victron"].as<JsonObject>(),
                config.PowerMeter.UdpVictron);
    }

    WebApi.writeConfig(retMsg);

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    PowerMeter.updateSettings();
}

void WebApiPowerMeterClass::onTestHttpJsonRequest(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* asyncJsonResponse = new AsyncJsonResponse();
    JsonDocument root;
    if (!WebApi.parseRequestData(request, asyncJsonResponse, root)) {
        return;
    }

    auto& retMsg = asyncJsonResponse->getRoot();

    char response[256];

    auto powerMeterConfig = std::make_unique<PowerMeterHttpJsonConfig>();
    Configuration.deserializePowerMeterHttpJsonConfig(root["http_json"].as<JsonObject>(),
            *powerMeterConfig);
    auto upMeter = std::make_unique<::PowerMeters::Json::Http::Provider>(*powerMeterConfig);
    upMeter->init();
    auto res = upMeter->poll();
    using values_t = ::PowerMeters::DataPointContainer;
    if (std::holds_alternative<values_t>(res)) {
        retMsg["type"] = "success";
        auto const& vals = std::get<values_t>(res);
        auto iter = vals.cbegin();
        auto pos = snprintf(response, sizeof(response), "Result: %sW", iter->second.getValueText().c_str());
        ++iter;
        while (iter != vals.cend()) {
            pos += snprintf(response + pos, sizeof(response) - pos, ", %sW", iter->second.getValueText().c_str());
            ++iter;
        }
        snprintf(response + pos, sizeof(response) - pos, ", Total: %5.2f", upMeter->getPowerTotal());
    } else {
        snprintf(response, sizeof(response), "%s", std::get<String>(res).c_str());
    }

    retMsg["message"] = response;
    asyncJsonResponse->setLength();
    request->send(asyncJsonResponse);
}

void WebApiPowerMeterClass::onTestHttpSmlRequest(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* asyncJsonResponse = new AsyncJsonResponse();
    JsonDocument root;
    if (!WebApi.parseRequestData(request, asyncJsonResponse, root)) {
        return;
    }

    auto& retMsg = asyncJsonResponse->getRoot();

    char response[256];

    auto powerMeterConfig = std::make_unique<PowerMeterHttpSmlConfig>();
    Configuration.deserializePowerMeterHttpSmlConfig(root["http_sml"].as<JsonObject>(),
            *powerMeterConfig);
    auto upMeter = std::make_unique<::PowerMeters::Sml::Http::Provider>(*powerMeterConfig);
    upMeter->init();
    auto res = upMeter->poll();
    if (res.isEmpty()) {
        retMsg["type"] = "success";
        snprintf(response, sizeof(response), "Result: %5.2fW", upMeter->getPowerTotal());
    } else {
        snprintf(response, sizeof(response), "%s", res.c_str());
    }

    retMsg["message"] = response;
    asyncJsonResponse->setLength();
    request->send(asyncJsonResponse);
}
