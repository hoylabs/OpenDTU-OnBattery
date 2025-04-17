// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#include "WebApi_gridcharger.h"
#include <gridcharger/huawei/Controller.h>
#include "Configuration.h"
#include "MessageOutput.h"
#include "PinMapping.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include <AsyncJson.h>
#include <Hoymiles.h>

void WebApiGridChargerClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    _server = &server;

    _server->on("/api/gridcharger/status", HTTP_GET, std::bind(&WebApiGridChargerClass::onStatus, this, _1));
    _server->on("/api/gridcharger/config", HTTP_GET, std::bind(&WebApiGridChargerClass::onAdminGet, this, _1));
    _server->on("/api/gridcharger/config", HTTP_POST, std::bind(&WebApiGridChargerClass::onAdminPost, this, _1));
    _server->on("/api/gridcharger/limit", HTTP_POST, std::bind(&WebApiGridChargerClass::onLimitPost, this, _1));
    _server->on("/api/gridcharger/power", HTTP_POST, std::bind(&WebApiGridChargerClass::onPowerPost, this, _1));
}

void WebApiGridChargerClass::onStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    HuaweiCan.getJsonData(root);

    response->setLength();
    request->send(response);
}

void WebApiGridChargerClass::onLimitPost(AsyncWebServerRequest* request)
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

    using Setting = GridChargers::Huawei::HardwareInterface::Setting;

    auto applySetting = [&](const char* key, float min, float max, WebApiError error, Setting setting) -> bool {
        if (!root[key].is<float>()) { return true; }

        auto value = root[key].as<float>();
        if (value < min || value > max) {
            retMsg["message"] = String(key) + " out of range [" +
                String(min) + ", " + String(max) + "]";
            retMsg["code"] = error;
            retMsg["param"]["min"] = min;
            retMsg["param"]["max"] = max;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return false;
        }

        HuaweiCan.setParameter(value, setting);
        return true;
    };

    using Controller = GridChargers::Huawei::Controller;

    if (!applySetting("voltage",
        Controller::MIN_ONLINE_VOLTAGE,
        Controller::MAX_ONLINE_VOLTAGE,
        WebApiError::R48xxVoltageLimitOutOfRange,
        Setting::OnlineVoltage)) {
        return;
    }

    if (!applySetting("current",
        Controller::MIN_ONLINE_CURRENT,
        Controller::MAX_ONLINE_CURRENT,
        WebApiError::R48xxCurrentLimitOutOfRange,
        Setting::OnlineCurrent)) {
        return;
    }

    retMsg["type"] = "success";
    retMsg["message"] = "Limits applied!";
    retMsg["code"] = WebApiError::GenericSuccess;

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiGridChargerClass::onPowerPost(AsyncWebServerRequest* request)
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

    if (!root["power"].is<bool>()) {
        retMsg["message"] = "Value missing!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    bool power = root["power"].as<bool>();
    HuaweiCan.setProduction(power);

    retMsg["type"] = "success";
    retMsg["message"] = "Power production " + String(power ? "en" : "dis") + "abled!";
    retMsg["code"] = WebApiError::GenericSuccess;

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiGridChargerClass::onAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto root = response->getRoot().as<JsonObject>();
    auto const& config = Configuration.get();

    ConfigurationClass::serializeGridChargerConfig(config.GridCharger, root);

    auto can = root["can"].to<JsonObject>();
    ConfigurationClass::serializeGridChargerCanConfig(config.GridCharger.Can, can);

    auto huawei = root["huawei"].to<JsonObject>();
    ConfigurationClass::serializeGridChargerHuaweiConfig(config.GridCharger.Huawei, huawei);

    response->setLength();
    request->send(response);
}

void WebApiGridChargerClass::onAdminPost(AsyncWebServerRequest* request)
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

    if (!(root["enabled"].is<bool>()) ||
        !(root["provider"].is<uint8_t>()) ||
        !(root["can"]["controller_frequency"].is<uint32_t>()) ||
        !(root["auto_power_enabled"].is<bool>()) ||
        !(root["emergency_charge_enabled"].is<bool>()) ||
        !(root["huawei"]["offline_voltage"].is<float>()) ||
        !(root["huawei"]["offline_current"].is<float>()) ||
        !(root["huawei"]["input_current_limit"].is<float>()) ||
        !(root["huawei"]["fan_online_full_speed"].is<bool>()) ||
        !(root["huawei"]["fan_offline_full_speed"].is<bool>()) ||
        !(root["voltage_limit"].is<float>()) ||
        !(root["lower_power_limit"].is<float>()) ||
        !(root["upper_power_limit"].is<float>())) {
        retMsg["message"] = "Values are missing or of wrong type!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        response->setLength();
        request->send(response);
        return;
    }

    using Controller = GridChargers::Huawei::Controller;

    auto isValidRange = [&](const char* valueName, float min, float max, WebApiError error) -> bool {
        if (root["huawei"][valueName].as<float>() < min || root["huawei"][valueName].as<float>() > max) {
            retMsg["message"] = String(valueName) + " out of range [" + String(min) + ", " + String(max) + "]";
            retMsg["code"] = error;
            retMsg["param"]["min"] = min;
            retMsg["param"]["max"] = max;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return false;
        }
        return true;
    };

    if (!isValidRange("offline_voltage", Controller::MIN_OFFLINE_VOLTAGE, Controller::MAX_OFFLINE_VOLTAGE, WebApiError::R48xxVoltageLimitOutOfRange) ||
        !isValidRange("offline_current", Controller::MIN_OFFLINE_CURRENT, Controller::MAX_OFFLINE_CURRENT, WebApiError::R48xxCurrentLimitOutOfRange) ||
        !isValidRange("input_current_limit", Controller::MIN_INPUT_CURRENT_LIMIT, Controller::MAX_INPUT_CURRENT_LIMIT, WebApiError::R48xxCurrentLimitOutOfRange)) {
        return;
    }

    {
        auto guard = Configuration.getWriteGuard();
        auto& config = guard.getConfig();
        ConfigurationClass::deserializeGridChargerConfig(root.as<JsonObject>(), config.GridCharger);
        ConfigurationClass::deserializeGridChargerCanConfig(root["can"].as<JsonObject>(), config.GridCharger.Can);
        ConfigurationClass::deserializeGridChargerHuaweiConfig(root["huawei"].as<JsonObject>(), config.GridCharger.Huawei);
    }

    WebApi.writeConfig(retMsg);

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    HuaweiCan.updateSettings();
}
