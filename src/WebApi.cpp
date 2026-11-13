// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Thomas Basler and others
 */
#include "WebApi.h"
#include "Configuration.h"
#include "defaults.h"
#include <AsyncJson.h>
#include <lwip/priv/tcpip_priv.h>
#include <lwip/tcp.h>

#undef TAG
static const char* TAG = "webapi";

WebApiClass::WebApiClass()
    : _server(HTTP_PORT)
{
}

void WebApiClass::init(Scheduler& scheduler)
{
    _webApiDevice.init(_server, scheduler);
    _webApiDevInfo.init(_server, scheduler);
    _webApiDtu.init(_server, scheduler);
    _webApiEventlog.init(_server, scheduler);
    _webApiFile.init(_server, scheduler);
    _webApiFirmware.init(_server, scheduler);
    _webApiGridprofile.init(_server, scheduler);
    _webApiI18n.init(_server, scheduler);
    _webApiInverter.init(_server, scheduler);
    _webApiLimit.init(_server, scheduler);
    _webApiLogging.init(_server, scheduler);
    _webApiMaintenance.init(_server, scheduler);
    _webApiMqtt.init(_server, scheduler);
    _webApiNetwork.init(_server, scheduler);
    _webApiNtp.init(_server, scheduler);
    _webApiPower.init(_server, scheduler);
    _webApiPrometheus.init(_server, scheduler);
    _webApiSecurity.init(_server, scheduler);
    _webApiSysstatus.init(_server, scheduler);
    _webApiWebapp.init(_server, scheduler);
    _webApiWsConsole.init(_server, scheduler);
    _webApiWsLive.init(_server, scheduler);
    _webApiBattery.init(_server, scheduler);
    _webApiPowerMeter.init(_server, scheduler);
    _webApiPowerLimiter.init(_server, scheduler);
    _webApiWsSolarChargerLive.init(_server, scheduler);
    _webApiSolarCharger.init(_server, scheduler);
    _webApiWsGridChargerLive.init(_server, scheduler);
    _webApiGridCharger.init(_server, scheduler);
    _webApiModbusServer.init(_server, scheduler);
    _webApiWsBatteryLive.init(_server, scheduler);

    _server.begin();
}

void WebApiClass::reload()
{
    _webApiWsConsole.reload();
    _webApiWsLive.reload();
    _webApiWsBatteryLive.reload();
    _webApiWsSolarChargerLive.reload();
    _webApiWsGridChargerLive.reload();
}

bool WebApiClass::checkCredentials(AsyncWebServerRequest* request)
{
    auto const& config = Configuration.get();
    if (request->authenticate(AUTH_USERNAME, config.Security.Password)) {
        return true;
    }

    AsyncWebServerResponse* r = request->beginResponse(401);

    // WebAPI should set the X-Requested-With to prevent browser internal auth dialogs
    if (!request->hasHeader("X-Requested-With")) {
        r->addHeader(asyncsrv::T_WWW_AUTH, "Basic realm=\"Login Required\"");
    }
    request->send(r);

    return false;
}

bool WebApiClass::checkCredentialsReadonly(AsyncWebServerRequest* request)
{
    auto const& config = Configuration.get();
    if (config.Security.AllowReadonly) {
        return true;
    } else {
        return checkCredentials(request);
    }
}

void WebApiClass::sendTooManyRequests(AsyncWebServerRequest* request)
{
    auto response = request->beginResponse(429, asyncsrv::T_text_plain, "Too Many Requests");
    response->addHeader(asyncsrv::T_retry_after, "60");
    request->send(response);
}

// lwIP only has CONFIG_LWIP_MAX_ACTIVE_TCP (16) TCP PCBs for all connections
// (HTTP, websockets, Modbus TCP, MQTT, HTTP clients). Every web UI tab holds
// several websockets, so a few tabs used to exhaust the pool and every new
// connection was dropped. Websockets get the lowest TCP priority, so lwIP
// closes the oldest websocket instead (the web UI reconnects).
struct SetLowestPrioCall {
    tcpip_api_call_data call; // must be the first member
    AsyncClient* client;
};

static err_t setLowestPrio(tcpip_api_call_data* data)
{
    // runs in the lwIP thread, which also frees the pcb if the connection fails
    auto pcb = reinterpret_cast<SetLowestPrioCall*>(data)->client->pcb();
    if (pcb != nullptr) { tcp_setprio(pcb, TCP_PRIO_MIN); }
    return ERR_OK;
}

void WebApiClass::onWsClientEvent(AsyncWebSocketClient* client, AwsEventType type)
{
    if (type != WS_EVT_CONNECT) { return; }

    SetLowestPrioCall call {};
    call.client = client->client();
    tcpip_api_call(setLowestPrio, &call.call);
}

void WebApiClass::writeConfig(JsonVariant& retMsg, const WebApiError code, const String& message)
{
    if (!Configuration.write()) {
        retMsg["message"] = "Write failed!";
        retMsg["code"] = WebApiError::GenericWriteFailed;
    } else {
        retMsg["type"] = "success";
        retMsg["message"] = message;
        retMsg["code"] = code;
    }
}

bool WebApiClass::parseRequestData(AsyncWebServerRequest* request, AsyncJsonResponse* response, JsonDocument& json_document)
{
    auto& retMsg = response->getRoot();
    retMsg["type"] = "warning";

    if (!request->hasParam("data", true)) {
        retMsg["message"] = "No values found!";
        retMsg["code"] = WebApiError::GenericNoValueFound;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return false;
    }

    const String json = request->getParam("data", true)->value();
    const DeserializationError error = deserializeJson(json_document, json);
    if (error) {
        retMsg["message"] = "Failed to parse data!";
        retMsg["code"] = WebApiError::GenericParseError;
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return false;
    }

    return true;
}

uint64_t WebApiClass::parseSerialFromRequest(AsyncWebServerRequest* request, String param_name)
{
    if (request->hasParam(param_name)) {
        String s = request->getParam(param_name)->value();
        return strtoll(s.c_str(), NULL, 16);
    }

    return 0;
}

String WebApiClass::formatCommandStatus(const LastCommandSuccess status)
{
    if (status == LastCommandSuccess::CMD_OK) {
        return "Ok";
    }
    if (status == LastCommandSuccess::CMD_NOK) {
        return "Failure";
    }
    if (status == LastCommandSuccess::CMD_PENDING) {
        return "Pending";
    }
    return "Unknown";
}

bool WebApiClass::sendJsonResponse(AsyncWebServerRequest* request, AsyncJsonResponse* response, const char* function, const uint16_t line)
{
    bool ret_val = true;
    if (response->overflowed()) {
        auto& root = response->getRoot();

        root.clear();
        root["message"] = String("500 Internal Server Error: ") + function + ", " + line;
        root["code"] = WebApiError::GenericInternalServerError;
        root["type"] = "danger";
        response->setCode(500);
        ESP_LOGE(TAG, "WebResponse failed: %s, %" PRIu16 "", function, line);
        ret_val = false;
    }

    response->setLength();
    request->send(response);
    return ret_val;
}

WebApiClass WebApi;
