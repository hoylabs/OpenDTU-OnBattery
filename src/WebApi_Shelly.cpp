// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 HSS
 */
#include "WebApi_Shelly.h"
#include "Configuration.h"
#include "MessageOutput.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include <AsyncJson.h>
#include <Hoymiles.h>

void WebApiShellyClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    _server = &server;

    _server->on("/api/shelly/status", HTTP_GET, std::bind(&WebApiShellyClass::onStatus, this, _1));
    _server->on("/api/shelly/config", HTTP_GET, std::bind(&WebApiShellyClass::onAdminGet, this, _1));
    _server->on("/api/shelly/config", HTTP_POST, std::bind(&WebApiShellyClass::onAdminPost, this, _1));
}

void WebApiShellyClass::onStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();

    response->setLength();
    request->send(response);
}

void WebApiShellyClass::onAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    const CONFIG_T& config = Configuration.get();

    root["enabled"] = config.Shelly.Enabled;
    root["verbose_logging"] = config.Shelly.VerboseLogging;
    root["auto_power_batterysoc_limits_enabled"] = config.Shelly.Auto_Power_BatterySoC_Limits_Enabled;
    root["emergency_charge_enabled"] = config.Shelly.Emergency_Charge_Enabled;
    root["stop_batterysoc_threshold"] = config.Shelly.stop_batterysoc_threshold;
    root["ip"] = config.Shelly.ip;
    response->setLength();
    request->send(response);
    MessageOutput.println("Read Shelly AC charger config... ");
}

void WebApiShellyClass::onAdminPost(AsyncWebServerRequest* request)
{
    MessageOutput.println("Write Shelly AC charger config... 1");
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
        !(root["emergency_charge_enabled"].is<bool>())){
        retMsg["message"] = "Values are missing!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        response->setLength();
        request->send(response);
        return;
    }

    CONFIG_T& config = Configuration.get();
    config.Shelly.Enabled = root["enabled"].as<bool>();
    config.Shelly.VerboseLogging = root["verbose_logging"];
    config.Shelly.Auto_Power_BatterySoC_Limits_Enabled = root["auto_power_batterysoc_limits_enabled"].as<bool>();
    config.Shelly.Emergency_Charge_Enabled = root["emergency_charge_enabled"].as<bool>();
    config.Shelly.stop_batterysoc_threshold = root["stop_batterysoc_threshold"];
    strlcpy( config.Shelly.ip, root["ip"].as<String>().c_str(), sizeof(config.Shelly.ip));
    MessageOutput.println("Write Shelly AC charger config... 2 ");
    WebApi.writeConfig(retMsg);
    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);


    yield();
    delay(1000);
    yield();
    //ESP.restart();

    // Properly turn this on
    if (config.Shelly.Enabled) {
        MessageOutput.println("Initialize Shelly AC charger interface... ");
    }

    // Properly turn this off
    if (!config.Shelly.Enabled) {
      //HuaweiCan.setValue(0, HUAWEI_ONLINE_CURRENT);
      delay(500);
      //HuaweiCan.setMode(HUAWEI_MODE_OFF);
      return;
    }
}
