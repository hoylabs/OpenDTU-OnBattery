#include "WebApi_battery_guard.h"
#include "Configuration.h"
#include "BatteryGuard.h"
#include "defaults.h"
#include "WebApi.h"
#include <AsyncJson.h>
#include <battery/Controller.h>

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

    auto const& limits = root["limits"];
    limits["max_voltage_resolution"] = BatteryGuard.MAXIMUM_VOLTAGE_RESOLUTION * 1000.0f; // mV
    limits["max_current_resolution"] = BatteryGuard.MAXIMUM_CURRENT_RESOLUTION * 1000.0f; // mA
    limits["max_measurement_time_period"] = BatteryGuard.MAXIMUM_MEASUREMENT_TIME_PERIOD; // In milliseconds
    limits["max_v_i_time_stamp_delay"] = BatteryGuard.MAXIMUM_V_I_TIME_STAMP_DELAY; // In milliseconds
    limits["min_resistance_calculation_count"] = BatteryGuard.MINIMUM_RESISTANCE_CALC;

    auto const& values = root["values"];

    // Open circuit voltage
    auto oOpenCircuitVoltage = BatteryGuard.getOpenCircuitVoltage();
    values["open_circuit_voltage_calculated"] = oOpenCircuitVoltage.has_value();
    if (oOpenCircuitVoltage) {
        values["open_circuit_voltage"] = *oOpenCircuitVoltage;
    } else {
        values["open_circuit_voltage"] = 0;
    }

    values["uncompensated_voltage"] = Battery.getStats()->getVoltage();

    // Internal resistance (configured and calculated)
    auto oInternalResistance = BatteryGuard.getInternalResistance();
    values["internal_resistance_calculated"] = oInternalResistance.has_value() && BatteryGuard.isInternalResistanceCalculated();

    if (oInternalResistance && BatteryGuard.isInternalResistanceCalculated()) {
        values["resistance_calculated"] = *oInternalResistance * 1000.0f; // mOhm
    }

    values["resistance_configured"] = config.BatteryGuard.InternalResistance; // mOhm

    // Get resistance calculation details
    values["resistance_calculation_count"] = BatteryGuard.getResistanceCalculationCount();
    values["resistance_calculation_state"] = BatteryGuard.getResistanceCalculationState();

    // Resolution and timing information
    values["voltage_resolution"] = BatteryGuard.getVoltageResolution() * 1000.0f; // mV
    values["current_resolution"] = BatteryGuard.getCurrentResolution() * 1000.0f; // mA
    values["measurement_time_period"] = BatteryGuard.getMeasurementPeriod(); // In milliseconds
    values["v_i_time_stamp_delay"] = BatteryGuard.getVIStampDelay(); // In milliseconds

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
