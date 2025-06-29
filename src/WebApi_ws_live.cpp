// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2025 Thomas Basler and others
 */
#include "WebApi_ws_live.h"
#include "Datastore.h"
#include "Utils.h"
#include "WebApi.h"
#include <battery/Controller.h>
#include <battery/Stats.h>
#include <gridcharger/huawei/Controller.h>
#include <powermeter/Controller.h>
#include "defaults.h"
#include <solarcharger/Controller.h>
#include <AsyncJson.h>

#undef TAG
static const char* TAG = "webapi";

#ifndef PIN_MAPPING_REQUIRED
    #define PIN_MAPPING_REQUIRED 0
#endif

WebApiWsLiveClass::WebApiWsLiveClass()
    : _ws("/livedata")
    , _wsCleanupTask(1 * TASK_SECOND, TASK_FOREVER, std::bind(&WebApiWsLiveClass::wsCleanupTaskCb, this))
    , _sendDataTask(1 * TASK_SECOND, TASK_FOREVER, std::bind(&WebApiWsLiveClass::sendDataTaskCb, this))
{
}

void WebApiWsLiveClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    server.on("/api/livedata/status", HTTP_GET, std::bind(&WebApiWsLiveClass::onLivedataStatus, this, _1));

    server.addHandler(&_ws);
    _ws.onEvent(std::bind(&WebApiWsLiveClass::onWebsocketEvent, this, _1, _2, _3, _4, _5, _6));

    scheduler.addTask(_wsCleanupTask);
    _wsCleanupTask.enable();

    scheduler.addTask(_sendDataTask);
    _sendDataTask.enable();
    _simpleDigestAuth.setUsername(AUTH_USERNAME);
    _simpleDigestAuth.setRealm("live websocket");

    reload();
}

void WebApiWsLiveClass::reload()
{
    _ws.removeMiddleware(&_simpleDigestAuth);

    auto const& config = Configuration.get();

    if (config.Security.AllowReadonly) {
        return;
    }

    _ws.enable(false);
    _simpleDigestAuth.setPassword(config.Security.Password);
    _ws.addMiddleware(&_simpleDigestAuth);
    _ws.closeAll();
    _ws.enable(true);
}

void WebApiWsLiveClass::wsCleanupTaskCb()
{
    // see: https://github.com/me-no-dev/ESPAsyncWebServer#limiting-the-number-of-web-socket-clients
    _ws.cleanupClients();
}

void WebApiWsLiveClass::generateOnBatteryJsonResponse(JsonVariant& root, bool all)
{
    auto const& config = Configuration.get();
    auto constexpr halfOfAllMillis = std::numeric_limits<uint32_t>::max() / 2;

    auto solarChargerAge = SolarCharger.getStats()->getAgeMillis();
    if (all || (solarChargerAge > 0 && (millis() - _lastPublishSolarCharger) > solarChargerAge)) {
        auto solarchargerObj = root["solarcharger"].to<JsonObject>();
        solarchargerObj["enabled"] = config.SolarCharger.Enabled;

        if (config.SolarCharger.Enabled) {
            float power = 0;
            auto outputPower = SolarCharger.getStats()->getOutputPowerWatts();
            auto panelPower = SolarCharger.getStats()->getPanelPowerWatts();

            if (outputPower) {
                power = *outputPower;
            }

            if (power == 0 && panelPower) {
                power = *panelPower;
            }

            addTotalField(solarchargerObj, "power", power, "W", 1);

            auto yieldDay = SolarCharger.getStats()->getYieldDay();
            if (yieldDay) {
                addTotalField(solarchargerObj, "yieldDay", *yieldDay, "Wh", 0);
            }

            auto yieldTotal = SolarCharger.getStats()->getYieldTotal();
            if (yieldTotal) {
                addTotalField(solarchargerObj, "yieldTotal", *yieldTotal, "kWh", 2);
            }
        }

        if (!all) { _lastPublishSolarCharger = millis(); }
    }

    if (all || (HuaweiCan.getDataPoints().getLastUpdate() - _lastPublishGridCharger) < halfOfAllMillis ) {
        auto gridChargerObj = root["gridcharger"].to<JsonObject>();
        gridChargerObj["enabled"] = config.GridCharger.Enabled;

        if (config.GridCharger.Enabled) {
            auto const& dataPoints = HuaweiCan.getDataPoints();
            auto oInputPower = dataPoints.get<GridChargers::Huawei::DataPointLabel::InputPower>();
            float pwr = oInputPower.value_or(0.0f);
            addTotalField(gridChargerObj, "Power", pwr, "W", 2);
        }

        if (!all) { _lastPublishGridCharger = millis(); }
    }

    auto spStats = Battery.getStats();
    if (all || spStats->updateAvailable(_lastPublishBattery)) {
        auto batteryObj = root["battery"].to<JsonObject>();
        batteryObj["enabled"] = config.Battery.Enabled;

        if (config.Battery.Enabled) {
            if (spStats->isSoCValid()) {
                addTotalField(batteryObj, "soc", spStats->getSoC(), "%", spStats->getSoCPrecision());
            }

            if (spStats->isVoltageValid()) {
                addTotalField(batteryObj, "voltage", spStats->getVoltage(), "V", 2);
            }

            if (spStats->isCurrentValid()) {
                addTotalField(batteryObj, "current", spStats->getChargeCurrent(), "A", spStats->getChargeCurrentPrecision());
            }

            if (spStats->isVoltageValid() && spStats->isCurrentValid()) {
                addTotalField(batteryObj, "power", spStats->getVoltage() * spStats->getChargeCurrent(), "W", 1);
            }
        }

        if (!all) { _lastPublishBattery = millis(); }
    }

    if (all || (PowerMeter.getLastUpdate() - _lastPublishPowerMeter) < halfOfAllMillis) {
        auto powerMeterObj = root["power_meter"].to<JsonObject>();
        powerMeterObj["enabled"] = config.PowerMeter.Enabled;

        if (config.PowerMeter.Enabled) {
            addTotalField(powerMeterObj, "Power", PowerMeter.getPowerTotal(), "W", 1);
        }

        if (!all) { _lastPublishPowerMeter = millis(); }
    }
}

void WebApiWsLiveClass::sendOnBatteryStats()
{
    JsonDocument root;
    JsonVariant var = root;

    bool all = (millis() - _lastPublishOnBatteryFull) > 10 * 1000;
    if (all) { _lastPublishOnBatteryFull = millis(); }
    generateOnBatteryJsonResponse(var, all);

    if (root.isNull()) { return; }

    if (Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
        String buffer;
        serializeJson(root, buffer);

        _ws.textAll(buffer);;
    }
}

void WebApiWsLiveClass::sendDataTaskCb()
{
    // do nothing if no WS client is connected
    if (_ws.count() == 0) {
        return;
    }

    sendOnBatteryStats();

    // Loop all inverters
    for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {
        auto inv = Hoymiles.getInverterByPos(i);
        if (inv == nullptr) {
            continue;
        }

        const uint32_t lastUpdateInternal = inv->Statistics()->getLastUpdateFromInternal();
        if (!((lastUpdateInternal > 0 && lastUpdateInternal > _lastPublishStats[i]) || (millis() - _lastPublishStats[i] > (10 * 1000)))) {
            continue;
        }

        _lastPublishStats[i] = millis();

        try {
            std::lock_guard<std::mutex> lock(_mutex);
            JsonDocument root;
            JsonVariant var = root;

            auto invArray = var["inverters"].to<JsonArray>();
            auto invObject = invArray.add<JsonObject>();

            generateCommonJsonResponse(var);
            generateInverterCommonJsonResponse(invObject, inv);
            generateInverterChannelJsonResponse(invObject, inv);

            if (!Utils::checkJsonAlloc(root, __FUNCTION__, __LINE__)) {
                continue;
            }

            String buffer;
            serializeJson(root, buffer);

            _ws.textAll(buffer);

        } catch (const std::bad_alloc& bad_alloc) {
            ESP_LOGE(TAG, "Call to /api/livedata/status temporarely out of resources. Reason: \"%s\".", bad_alloc.what());
        } catch (const std::exception& exc) {
            ESP_LOGE(TAG, "Unknown exception in /api/livedata/status. Reason: \"%s\".", exc.what());
        }
    }
}

void WebApiWsLiveClass::generateCommonJsonResponse(JsonVariant& root)
{
    auto totalObj = root["total"].to<JsonObject>();
    addTotalField(totalObj, "Power", Datastore.getTotalAcPowerEnabled(), "W", Datastore.getTotalAcPowerDigits());
    addTotalField(totalObj, "YieldDay", Datastore.getTotalAcYieldDayEnabled(), "Wh", Datastore.getTotalAcYieldDayDigits());
    addTotalField(totalObj, "YieldTotal", Datastore.getTotalAcYieldTotalEnabled(), "kWh", Datastore.getTotalAcYieldTotalDigits());

    JsonObject hintObj = root["hints"].to<JsonObject>();
    struct tm timeinfo;
    hintObj["time_sync"] = !getLocalTime(&timeinfo, 5);
    hintObj["radio_problem"] = (Hoymiles.getRadioNrf()->isInitialized() && (!Hoymiles.getRadioNrf()->isConnected() || !Hoymiles.getRadioNrf()->isPVariant())) || (Hoymiles.getRadioCmt()->isInitialized() && (!Hoymiles.getRadioCmt()->isConnected()));
    hintObj["default_password"] = strcmp(Configuration.get().Security.Password, ACCESS_POINT_PASSWORD) == 0;

    hintObj["pin_mapping_issue"] = PIN_MAPPING_REQUIRED && !PinMapping.isMappingSelected();
}

void WebApiWsLiveClass::generateInverterCommonJsonResponse(JsonObject& root, std::shared_ptr<InverterAbstract> inv)
{
    const INVERTER_CONFIG_T* inv_cfg = Configuration.getInverterConfig(inv->serial());
    if (inv_cfg == nullptr) {
        return;
    }

    root["serial"] = inv->serialString();
    root["name"] = inv->name();
    root["order"] = inv_cfg->Order;
    root["data_age"] = (millis() - inv->Statistics()->getLastUpdate()) / 1000;
    root["data_age_ms"] = millis() - inv->Statistics()->getLastUpdate();
    root["poll_enabled"] = inv->getEnablePolling();
    root["reachable"] = inv->isReachable();
    root["producing"] = inv->isProducing();
    root["limit_relative"] = inv->SystemConfigPara()->getLimitPercent();
    if (inv->DevInfo()->getMaxPower() > 0) {
        root["limit_absolute"] = inv->SystemConfigPara()->getLimitPercent() * inv->DevInfo()->getMaxPower() / 100.0;
    } else {
        root["limit_absolute"] = -1;
    }
    root["radio_stats"]["tx_request"] = inv->RadioStats.TxRequestData;
    root["radio_stats"]["tx_re_request"] = inv->RadioStats.TxReRequestFragment;
    root["radio_stats"]["rx_success"] = inv->RadioStats.RxSuccess;
    root["radio_stats"]["rx_fail_nothing"] = inv->RadioStats.RxFailNoAnswer;
    root["radio_stats"]["rx_fail_partial"] = inv->RadioStats.RxFailPartialAnswer;
    root["radio_stats"]["rx_fail_corrupt"] = inv->RadioStats.RxFailCorruptData;
    root["radio_stats"]["rssi"] = inv->getLastRssi();
}

void WebApiWsLiveClass::generateInverterChannelJsonResponse(JsonObject& root, std::shared_ptr<InverterAbstract> inv)
{
    const INVERTER_CONFIG_T* inv_cfg = Configuration.getInverterConfig(inv->serial());
    if (inv_cfg == nullptr) {
        return;
    }

    // Loop all channels
    for (auto& t : inv->Statistics()->getChannelTypes()) {
        auto chanTypeObj = root[inv->Statistics()->getChannelTypeName(t)].to<JsonObject>();
        for (auto& c : inv->Statistics()->getChannelsByType(t)) {
            if (t == TYPE_DC) {
                chanTypeObj[String(static_cast<uint8_t>(c))]["name"]["u"] = inv_cfg->channel[c].Name;
            }
            addField(chanTypeObj, inv, t, c, FLD_PAC);
            addField(chanTypeObj, inv, t, c, FLD_UAC);
            addField(chanTypeObj, inv, t, c, FLD_IAC);
            if (t == TYPE_INV) {
                addField(chanTypeObj, inv, t, c, FLD_PDC, "Power DC");
            } else {
                addField(chanTypeObj, inv, t, c, FLD_PDC);
            }
            addField(chanTypeObj, inv, t, c, FLD_UDC);
            addField(chanTypeObj, inv, t, c, FLD_IDC);
            addField(chanTypeObj, inv, t, c, FLD_YD);
            addField(chanTypeObj, inv, t, c, FLD_YT);
            addField(chanTypeObj, inv, t, c, FLD_F);
            addField(chanTypeObj, inv, t, c, FLD_T);
            addField(chanTypeObj, inv, t, c, FLD_PF);
            addField(chanTypeObj, inv, t, c, FLD_Q);
            addField(chanTypeObj, inv, t, c, FLD_EFF);
            if (t == TYPE_DC && inv->Statistics()->getStringMaxPower(c) > 0) {
                addField(chanTypeObj, inv, t, c, FLD_IRR);
                chanTypeObj[String(c)][inv->Statistics()->getChannelFieldName(t, c, FLD_IRR)]["max"] = inv->Statistics()->getStringMaxPower(c);
            }
        }
    }

    if (inv->Statistics()->hasChannelFieldValue(TYPE_INV, CH0, FLD_EVT_LOG)) {
        root["events"] = inv->EventLog()->getEntryCount();
    } else {
        root["events"] = -1;
    }
}

void WebApiWsLiveClass::addField(JsonObject& root, std::shared_ptr<InverterAbstract> inv, const ChannelType_t type, const ChannelNum_t channel, const FieldId_t fieldId, String topic)
{
    if (inv->Statistics()->hasChannelFieldValue(type, channel, fieldId)) {
        String chanName;
        if (topic == "") {
            chanName = inv->Statistics()->getChannelFieldName(type, channel, fieldId);
        } else {
            chanName = topic;
        }
        String chanNum;
        chanNum = channel;
        root[chanNum][chanName]["v"] = inv->Statistics()->getChannelFieldValue(type, channel, fieldId);
        root[chanNum][chanName]["u"] = inv->Statistics()->getChannelFieldUnit(type, channel, fieldId);
        root[chanNum][chanName]["d"] = inv->Statistics()->getChannelFieldDigits(type, channel, fieldId);
    }
}

void WebApiWsLiveClass::addTotalField(JsonObject& root, const String& name, const float value, const String& unit, const uint8_t digits)
{
    root[name]["v"] = value;
    root[name]["u"] = unit;
    root[name]["d"] = digits;
}

void WebApiWsLiveClass::onWebsocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len)
{
    if (type == WS_EVT_CONNECT) {
        ESP_LOGD(TAG, "Websocket: [%s][%" PRIu32 "] connect", server->url(), client->id());
    } else if (type == WS_EVT_DISCONNECT) {
        ESP_LOGD(TAG, "Websocket: [%s][%" PRIu32 "] disconnect", server->url(), client->id());
    }
}

void WebApiWsLiveClass::onLivedataStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    try {
        std::lock_guard<std::mutex> lock(_mutex);
        AsyncJsonResponse* response = new AsyncJsonResponse();
        auto& root = response->getRoot();
        auto invArray = root["inverters"].to<JsonArray>();
        auto serial = WebApi.parseSerialFromRequest(request);

        if (serial > 0) {
            auto inv = Hoymiles.getInverterBySerial(serial);
            if (inv != nullptr) {
                JsonObject invObject = invArray.add<JsonObject>();
                generateInverterCommonJsonResponse(invObject, inv);
                generateInverterChannelJsonResponse(invObject, inv);
            }
        } else {
            // Loop all inverters
            for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {
                auto inv = Hoymiles.getInverterByPos(i);
                if (inv == nullptr) {
                    continue;
                }

                JsonObject invObject = invArray.add<JsonObject>();
                generateInverterCommonJsonResponse(invObject, inv);
            }
        }

        generateCommonJsonResponse(root);

        generateOnBatteryJsonResponse(root, true);

        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);

    } catch (const std::bad_alloc& bad_alloc) {
        ESP_LOGE(TAG, "Call to /api/livedata/status temporarely out of resources. Reason: \"%s\".", bad_alloc.what());
        WebApi.sendTooManyRequests(request);
    } catch (const std::exception& exc) {
        ESP_LOGE(TAG, "Unknown exception in /api/livedata/status. Reason: \"%s\".", exc.what());
        WebApi.sendTooManyRequests(request);
    }
}
