// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "WebApi_battery.h"
#include "WebApi_device.h"
#include "WebApi_devinfo.h"
#include "WebApi_dtu.h"
#include "WebApi_errors.h"
#include "WebApi_eventlog.h"
#include "WebApi_file.h"
#include "WebApi_firmware.h"
#include "WebApi_gridprofile.h"
#include "WebApi_i18n.h"
#include "WebApi_inverter.h"
#include "WebApi_limit.h"
#include "WebApi_logging.h"
#include "WebApi_maintenance.h"
#include "WebApi_mqtt.h"
#include "WebApi_network.h"
#include "WebApi_ntp.h"
#include "WebApi_power.h"
#include "WebApi_powermeter.h"
#include "WebApi_powerlimiter.h"
#include "WebApi_prometheus.h"
#include "WebApi_security.h"
#include "WebApi_sysstatus.h"
#include "WebApi_webapp.h"
#include "WebApi_ws_console.h"
#include "WebApi_ws_live.h"
#include <AsyncJson.h>
#include "WebApi_ws_solarcharger_live.h"
#include "WebApi_solarcharger.h"
#include "WebApi_ws_gridcharger.h"
#include "WebApi_gridcharger.h"
#include "WebApi_ws_battery.h"
#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>

class WebApiClass {
public:
    WebApiClass();
    void init(Scheduler& scheduler);
    void reload();

    static bool checkCredentials(AsyncWebServerRequest* request);
    static bool checkCredentialsReadonly(AsyncWebServerRequest* request);

    static void sendTooManyRequests(AsyncWebServerRequest* request);

    static void writeConfig(JsonVariant& retMsg, const WebApiError code = WebApiError::GenericSuccess, const String& message = "Settings saved!");

    static bool parseRequestData(AsyncWebServerRequest* request, AsyncJsonResponse* response, JsonDocument& json_document);
    static uint64_t parseSerialFromRequest(AsyncWebServerRequest* request, String param_name = "inv");
    static bool sendJsonResponse(AsyncWebServerRequest* request, AsyncJsonResponse* response, const char* function, const uint16_t line);

private:
    AsyncWebServer _server;

    WebApiBatteryClass _webApiBattery;
    WebApiDeviceClass _webApiDevice;
    WebApiDevInfoClass _webApiDevInfo;
    WebApiDtuClass _webApiDtu;
    WebApiEventlogClass _webApiEventlog;
    WebApiFileClass _webApiFile;
    WebApiFirmwareClass _webApiFirmware;
    WebApiGridProfileClass _webApiGridprofile;
    WebApiI18nClass _webApiI18n;
    WebApiInverterClass _webApiInverter;
    WebApiLimitClass _webApiLimit;
    WebApiLoggingClass _webApiLogging;
    WebApiMaintenanceClass _webApiMaintenance;
    WebApiMqttClass _webApiMqtt;
    WebApiNetworkClass _webApiNetwork;
    WebApiNtpClass _webApiNtp;
    WebApiPowerClass _webApiPower;
    WebApiPowerMeterClass _webApiPowerMeter;
    WebApiPowerLimiterClass _webApiPowerLimiter;
    WebApiPrometheusClass _webApiPrometheus;
    WebApiSecurityClass _webApiSecurity;
    WebApiSysstatusClass _webApiSysstatus;
    WebApiWebappClass _webApiWebapp;
    WebApiWsConsoleClass _webApiWsConsole;
    WebApiWsLiveClass _webApiWsLive;
    WebApiWsSolarChargerLiveClass _webApiWsSolarChargerLive;
    WebApiSolarChargerlass _webApiSolarCharger;
    WebApiGridChargerClass _webApiGridCharger;
    WebApiWsGridChargerLiveClass _webApiWsGridChargerLive;
    WebApiWsBatteryLiveClass _webApiWsBatteryLive;
};

extern WebApiClass WebApi;
