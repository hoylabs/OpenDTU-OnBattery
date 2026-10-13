// SPDX-License-Identifier: GPL-2.0-or-later
#include "WebApi_modbusserver.h"
#include "Configuration.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include "defaults.h"
#include "modbus/ModbusServer.h"
#include <AsyncJson.h>
#include <Hoymiles.h>
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
    _server->on("/api/modbusserver/metadata", HTTP_GET,
        static_cast<ArRequestHandlerFunction>(std::bind(&WebApiModbusServerClass::onMetaData, this, _1)));
}

void WebApiModbusServerClass::onMetaData(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    auto const& config = Configuration.get();

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();
    JsonArray inverters = root["inverters"].to<JsonArray>();

    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        auto inv = Hoymiles.getInverterBySerial(config.Inverter[i].Serial);
        if (!inv) { continue; }

        JsonObject obj = inverters.add<JsonObject>();
        obj["serial"] = inv->serialString();
        obj["name"] = String(config.Inverter[i].Name);
        obj["type"] = inv->typeName();
    }

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
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

    // Match what deserializeModbusServerConfig() and the web UI (unit ID
    // input has min=1 max=247) each require, so a malformed entry is
    // rejected here with a clear error instead of silently being dropped
    // or defaulted during persistence.
    // Serial doubles as the "end of list" terminator everywhere else
    // (Serial == 0 means "no more entries"), so a zero/unparseable serial
    // anywhere but the last entry would silently hide every entry after
    // it - reject the whole request rather than let that happen.
    std::set<uint8_t> seenUnitIds;
    for (JsonObject inv : root["inverter"].as<JsonArray>()) {
        if (!inv["serial"].is<const char*>() || !inv["unit_id"].is<uint8_t>()) {
            retMsg["message"] = "Inverter entry is missing a serial or unit ID!";
            retMsg["code"] = WebApiError::GenericValueMissing;
            response->setLength();
            request->send(response);
            return;
        }

        if (strtoll(inv["serial"].as<const char*>(), nullptr, 16) == 0) {
            retMsg["message"] = "Invalid serial: must be a non-zero hex value!";
            retMsg["code"] = WebApiError::GenericValueMissing;
            response->setLength();
            request->send(response);
            return;
        }

        uint8_t unitId = inv["unit_id"].as<uint8_t>();
        if (unitId < 1 || unitId > 247) {
            retMsg["message"] = "Unit ID must be between 1 and 247!";
            retMsg["code"] = WebApiError::GenericValueMissing;
            response->setLength();
            request->send(response);
            return;
        }

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
