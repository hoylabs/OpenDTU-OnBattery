// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */
#include "WebApi_Huawei.h"
#include <gridcharger/huawei/Controller.h>
#include "Configuration.h"
#include "MessageOutput.h"
#include "PinMapping.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include <AsyncJson.h>
#include <Hoymiles.h>

void WebApiHuaweiClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    _server = &server;

    _server->on("/api/huawei/status", HTTP_GET, std::bind(&WebApiHuaweiClass::onStatus, this, _1));
    _server->on("/api/huawei/config", HTTP_GET, std::bind(&WebApiHuaweiClass::onAdminGet, this, _1));
    _server->on("/api/huawei/config", HTTP_POST, std::bind(&WebApiHuaweiClass::onAdminPost, this, _1));
    _server->on("/api/huawei/limit", HTTP_POST, std::bind(&WebApiHuaweiClass::onLimitPost, this, _1));
    _server->on("/api/huawei/power", HTTP_POST, std::bind(&WebApiHuaweiClass::onPowerPost, this, _1));
}

void WebApiHuaweiClass::onStatus(AsyncWebServerRequest* request)
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

void WebApiHuaweiClass::onLimitPost(AsyncWebServerRequest* request)
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

    using Setting = GridCharger::Huawei::HardwareInterface::Setting;

    if (root.containsKey("voltage") && root["voltage"].is<float>()) {
        auto value = root["voltage"].as<float>();
        if (value < HUAWEI_MINIMAL_ONLINE_VOLTAGE || value > 58) {
            retMsg["message"] = "voltage out of range [" + String(HUAWEI_MINIMAL_ONLINE_VOLTAGE) + ", 58]";
            retMsg["code"] = WebApiError::R48xxVoltageLimitOutOfRange;
            retMsg["param"]["min"] = HUAWEI_MINIMAL_ONLINE_VOLTAGE;
            retMsg["param"]["max"] = 58;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }

        HuaweiCan.setParameter(value, Setting::OnlineVoltage);
    }

    if (root.containsKey("current") && root["current"].is<float>()) {
        auto value = root["current"].as<float>();
        if (value < 0 || value > 75) {
            retMsg["message"] = "current out of range [0, 75]";
            retMsg["code"] = WebApiError::R48xxCurrentLimitOutOfRange;
            retMsg["param"]["min"] = 0;
            retMsg["param"]["max"] = 75;
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }

        HuaweiCan.setParameter(value, Setting::OnlineCurrent);
    }

    retMsg["type"] = "success";
    retMsg["message"] = "Limits applied!";
    retMsg["code"] = WebApiError::GenericSuccess;

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiHuaweiClass::onPowerPost(AsyncWebServerRequest* request)
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

void WebApiHuaweiClass::onAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto root = response->getRoot().as<JsonObject>();
    auto const& config = Configuration.get();

    ConfigurationClass::serializeGridChargerConfig(config.Huawei, root);

    response->setLength();
    request->send(response);
}

void WebApiHuaweiClass::onAdminPost(AsyncWebServerRequest* request)
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
        !(root["can_controller_frequency"].is<uint32_t>()) ||
        !(root["auto_power_enabled"].is<bool>()) ||
        !(root["emergency_charge_enabled"].is<bool>()) ||
        !(root["voltage_limit"].is<float>()) ||
        !(root["lower_power_limit"].is<float>()) ||
        !(root["upper_power_limit"].is<float>())) {
        retMsg["message"] = "Values are missing!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        response->setLength();
        request->send(response);
        return;
    }

    {
        auto guard = Configuration.getWriteGuard();
        auto& config = guard.getConfig();
        ConfigurationClass::deserializeGridChargerConfig(root.as<JsonObject>(), config.Huawei);
    }

    WebApi.writeConfig(retMsg);

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    HuaweiCan.updateSettings();
}
