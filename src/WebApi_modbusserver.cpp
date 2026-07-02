// SPDX-License-Identifier: GPL-2.0-or-later
#include "WebApi_modbusserver.h"
#include "Configuration.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include "defaults.h"
#include "modbus/ModbusServer.h"
#include <AsyncJson.h>
#include <set>

WebApiModbusServerClass WebApiModbusServer;

void WebApiModbusServerClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    _server = &server;

    _server->on("/api/modbusserver/config", HTTP_GET,
        static_cast<ArRequestHandlerFunction>(std::bind(&WebApiModbusServerClass::onAdminGet, this, _1)));
    _server->on("/api/modbusserver/config", HTTP_POST,
        static_cast<ArRequestHandlerFunction>(std::bind(&WebApiModbusServerClass::onAdminPost, this, _1)));
}

void WebApiModbusServerClass::onAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto root = response->getRoot().as<JsonObject>();
    auto const& config = Configuration.get();

    ConfigurationClass::serializeModbusServerConfig(config.ModbusServer, root);

    response->setLength();
    request->send(response);
}

void WebApiModbusServerClass::onAdminPost(AsyncWebServerRequest* request)
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

    if (!root["enabled"].is<bool>() ||
        !root["port"].is<uint16_t>() ||
        !root["inverter"].is<JsonArray>()) {
        retMsg["message"] = "Values are missing or of wrong type!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        response->setLength();
        request->send(response);
        return;
    }

    uint16_t port = root["port"].as<uint16_t>();
    if (port == 0) {
        retMsg["message"] = "Invalid port!";
        retMsg["code"] = WebApiError::GenericValueMissing;
        response->setLength();
        request->send(response);
        return;
    }

    std::set<uint8_t> seenUnitIds;
    for (JsonObject inv : root["inverter"].as<JsonArray>()) {
        uint8_t unitId = inv["unit_id"] | (uint8_t)0;
        if (!seenUnitIds.insert(unitId).second) {
            retMsg["message"] = "Duplicate unit ID: each inverter needs a unique unit ID!";
            retMsg["code"] = WebApiError::ModbusServerDuplicateUnitId;
            response->setLength();
            request->send(response);
            return;
        }
    }

    {
        auto guard = Configuration.getWriteGuard();
        auto& config = guard.getConfig();
        ConfigurationClass::deserializeModbusServerConfig(root.as<JsonObject>(), config.ModbusServer);
    }

    WebApi.writeConfig(retMsg);
    ModbusServer.updateSettings();

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}
