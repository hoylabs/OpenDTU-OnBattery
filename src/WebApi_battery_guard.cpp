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

    _server->on("/api/batteryguard/status", HTTP_GET, std::bind(&WebApiBatteryGuardClass::onStatus, this, _1));
    _server->on("/api/batteryguard/config", HTTP_GET, std::bind(&WebApiBatteryGuardClass::onAdminGet, this, _1));
    _server->on("/api/batteryguard/config", HTTP_POST, std::bind(&WebApiBatteryGuardClass::onAdminPost, this, _1));
    _server->on("/api/batteryguard/metadata", HTTP_GET, std::bind(&WebApiBatteryGuardClass::onMetaData, this, _1));
}

void WebApiBatteryGuardClass::onStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto root = response->getRoot().as<JsonObject>();
    auto const& config = Configuration.get();

    // Basic configuration status
    root["enabled"] = config.BatteryGuard.Enabled;
    root["battery_data_sufficient"] = BatteryGuard.isResolutionOK();

    // Open circuit voltage
    auto openCircuitVoltage = BatteryGuard.getOpenCircuitVoltage();
    if (openCircuitVoltage.has_value()) {
        root["open_circuit_voltage"] = openCircuitVoltage.value();
    } else {
        root["open_circuit_voltage"] = 0;
    }

    // Internal resistance (configured and calculated)
    auto internalResistance = BatteryGuard.getInternalResistance();
    if (internalResistance.has_value()) {
        if (BatteryGuard.isInternalResistanceCalculated()) {
            root["resistance_calculated"] = internalResistance.value() * 1000.0f; // Convert from Ohm to mOhm
            root["resistance_configured"] = config.BatteryGuard.InternalResistance; // Already in mOhm
        } else {
            root["resistance_calculated"] = 0;
            root["resistance_configured"] = internalResistance.value() * 1000.0f; // Convert from Ohm to mOhm
        }
    } else {
        root["resistance_calculated"] = 0;
        root["resistance_configured"] = 0;
    }

    // Get resistance calculation details
    root["resistance_calculation_count"] = BatteryGuard.getResistanceCalculationCount();
    root["resistance_calculation_state"] = BatteryGuard.getResistanceCalculationState();

    // Resolution and timing information
    root["voltage_resolution"] = BatteryGuard.getVoltageResolution() * 1000.0f; // Convert to mV
    root["current_resolution"] = BatteryGuard.getCurrentResolution() * 1000.0f; // Convert to mA
    root["measurement_time_period"] = BatteryGuard.getMeasurementPeriod(); // In milliseconds
    root["v_i_time_stamp_delay"] = BatteryGuard.getVIStampDelay(); // In milliseconds

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
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
