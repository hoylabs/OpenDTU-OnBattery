#include "WebApi_battery_guard.h"
#include "Configuration.h"
#include "BatteryGuard.h"
#include "defaults.h"
#include "WebApi.h"
#include <AsyncJson.h>

void WebApiBatteryGuardClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    _server = &server;

    _server->on("/api/batteryguard/config", HTTP_GET, std::bind(&WebApiBatteryGuardClass::onAdminGet, this, std::placeholders::_1));
    _server->on("/api/batteryguard/config", HTTP_POST, std::bind(&WebApiBatteryGuardClass::onAdminPost, this, std::placeholders::_1));
    _server->on("/api/batteryguard/metadata", HTTP_GET, std::bind(&WebApiBatteryGuardClass::onMetaData, this, _1));
}

void WebApiBatteryGuardClass::onMetaData(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) { return; }

    auto const& config = Configuration.get();

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();

    root["battery_enabled"] = config.Battery.Enabled;

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiBatteryGuardClass::onAdminGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) { return; }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto root = response->getRoot().as<JsonObject>();
    auto const& config = Configuration.get();
    ConfigurationClass::serializeBatteryGuardConfig(config.BatteryGuard, root);
    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiBatteryGuardClass::onAdminPost(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) { return; }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonDocument root;
    if (!WebApi.parseRequestData(request, response, root)) {
        return;
    }

    auto& retMsg = response->getRoot();

    {
        auto guard = Configuration.getWriteGuard();
        auto& config = guard.getConfig();
        ConfigurationClass::deserializeBatteryGuardConfig(root.as<JsonObject>(), config.BatteryGuard);
    }

    WebApi.writeConfig(retMsg);

    response->setLength();
    request->send(response);

    BatteryGuard.updateSettings();
}
