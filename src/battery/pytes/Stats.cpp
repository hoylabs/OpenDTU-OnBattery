// SPDX-License-Identifier: GPL-2.0-or-later
#include <MqttSettings.h>
#include <battery/pytes/Stats.h>

namespace Batteries::Pytes {

void Stats::getLiveViewData(JsonVariant& root) const
{
    ::Batteries::Stats::getLiveViewData(root);

    // values go into the "Status" card of the web application
    addLiveViewValue(root, "chargeVoltage", _chargeVoltageLimit, "V", 1);
    addLiveViewValue(root, "dischargeVoltageLimitation", _dischargeVoltageLimit, "V", 1);
    addLiveViewValue(root, "stateOfHealth", _stateOfHealth, "%", 0);
    if (_chargeCycles != -1) {
        addLiveViewValue(root, "chargeCycles", _chargeCycles, "", 0);
    }

    auto oTemperature = getTemperature();
    if (oTemperature) {
        addLiveViewValue(root, "temperature", *oTemperature, "°C", 1);
    }

    addLiveViewValue(root, "capacity", _totalCapacity, "Ah", _capacityPrecision);
    addLiveViewValue(root, "availableCapacity", _availableCapacity, "Ah", _capacityPrecision);

    if (_chargedEnergy != -1) {
        addLiveViewValue(root, "chargedEnergy", _chargedEnergy, "kWh", 1);
    }

    if (_dischargedEnergy != -1) {
        addLiveViewValue(root, "dischargedEnergy", _dischargedEnergy, "kWh", 1);
    }
    if (_chargeEnabled) { addLiveViewTextValue(root, "chargeEnabled", (*_chargeEnabled?"yes":"no")); }
    if (_dischargeEnabled) { addLiveViewTextValue(root, "dischargeEnabled", (*_dischargeEnabled?"yes":"no")); }
    addLiveViewTextValue(root, "chargeImmediately", (_chargeImmediately?"yes":"no"));

    if (_balance != -1) {
        addLiveViewTextValue(root, "balancingActive", (_balance?"yes":"no"));
    }

    addLiveViewInSection(root, "cells", "cellMinVoltage", static_cast<float>(_cellMinMilliVolt)/1000, "V", 3);
    addLiveViewInSection(root, "cells", "cellMaxVoltage", static_cast<float>(_cellMaxMilliVolt)/1000, "V", 3);
    addLiveViewInSection(root, "cells", "cellDiffVoltage", (_cellMaxMilliVolt - _cellMinMilliVolt), "mV", 0);
    addLiveViewInSection(root, "cells", "cellMinTemperature", _cellMinTemperature, "°C", 0);
    addLiveViewInSection(root, "cells", "cellMaxTemperature", _cellMaxTemperature, "°C", 0);

    addLiveViewTextInSection(root, "cells", "cellMinVoltageName", _cellMinVoltageName.c_str(), false);
    addLiveViewTextInSection(root, "cells", "cellMaxVoltageName", _cellMaxVoltageName.c_str(), false);
    addLiveViewTextInSection(root, "cells", "cellMinTemperatureName", _cellMinTemperatureName.c_str(), false);
    addLiveViewTextInSection(root, "cells", "cellMaxTemperatureName", _cellMaxTemperatureName.c_str(), false);

    addLiveViewInSection(root, "modules", "online", _moduleCountOnline, "", 0);
    addLiveViewInSection(root, "modules", "offline", _moduleCountOffline, "", 0);
    addLiveViewInSection(root, "modules", "blockingCharge", _moduleCountBlockingCharge, "", 0);
    addLiveViewInSection(root, "modules", "blockingDischarge", _moduleCountBlockingDischarge, "", 0);
    if (_moduleCountHeating != -1) {
        addLiveViewInSection(root, "modules", "heating", _moduleCountHeating, "", 0);
    }

    // alarms and warnings go into the "Issues" card of the web application
    addLiveViewWarning(root, "highCurrentDischarge", _warningHighDischargeCurrent);
    addLiveViewAlarm(root, "overCurrentDischarge", _alarmOverCurrentDischarge);

    addLiveViewWarning(root, "highCurrentCharge", _warningHighChargeCurrent);
    addLiveViewAlarm(root, "overCurrentCharge", _alarmOverCurrentCharge);

    addLiveViewWarning(root, "lowVoltage", _warningLowVoltage);
    addLiveViewAlarm(root, "underVoltage", _alarmUnderVoltage);

    addLiveViewWarning(root, "highVoltage", _warningHighVoltage);
    addLiveViewAlarm(root, "overVoltage", _alarmOverVoltage);

    addLiveViewWarning(root, "lowTemperature", _warningLowTemperature);
    addLiveViewAlarm(root, "underTemperature", _alarmUnderTemperature);

    addLiveViewWarning(root, "highTemperature", _warningHighTemperature);
    addLiveViewAlarm(root, "overTemperature", _alarmOverTemperature);

    addLiveViewWarning(root, "lowTemperatureCharge", _warningLowTemperatureCharge);
    addLiveViewAlarm(root, "underTemperatureCharge", _alarmUnderTemperatureCharge);

    addLiveViewWarning(root, "highTemperatureCharge", _warningHighTemperatureCharge);
    addLiveViewAlarm(root, "overTemperatureCharge", _alarmOverTemperatureCharge);

    addLiveViewWarning(root, "bmsInternal", _warningInternalFailure);
    addLiveViewAlarm(root, "bmsInternal", _alarmInternalFailure);

    addLiveViewWarning(root, "cellDiffVoltage", _warningCellImbalance);
    addLiveViewAlarm(root, "cellDiffVoltage", _alarmCellImbalance);

    // summary for the module selector, the details of each module are sent
    // separately, see getLiveViewModuleData()
    auto ids = getModuleIdentities();
    if (ids.empty()) { return; }

    root["numberOfModules"] = static_cast<int>(ids.size());
    JsonArray modules = root["modules"].to<JsonArray>();
    for (auto const& id : ids) {
        JsonObject module = modules.add<JsonObject>();
        module["moduleNumber"] = id.number;
        module["moduleName"] = "Module " + String(id.number);
        module["online"] = isModuleOnline(id.number - 1);
    }
}

bool Stats::hasModuleData(uint8_t index) const
{
    return _modules[index].lastUpdate != 0;
}

// no data for a minute: with 16 modules, each one is requested every 16 s
bool Stats::isModuleOnline(uint8_t index) const
{
    return hasModuleData(index) && millis() - _modules[index].lastUpdate <= 60 * 1000;
}

std::vector<Stats::ModuleIdentity> Stats::getModuleIdentities() const
{
    std::vector<ModuleIdentity> ids;
    for (uint8_t i = 0; i < MaxModules; ++i) {
        if (!hasModuleData(i)) { continue; }
        ids.push_back({ static_cast<uint8_t>(i + 1), _modules[i].cellCount, _modules[i].fwVersion });
    }
    return ids;
}

size_t Stats::getLiveViewModuleCount() const
{
    return MaxModules;
}

// arrays as described by moduleColumns in the web application's
// BatteryView.vue, null: not reported via CAN
bool Stats::getLiveViewModuleData(JsonVariant& root, size_t index, uint32_t since) const
{
    if (index >= MaxModules || !hasModuleData(index)) { return false; }
    auto const& mod = _modules[index];
    if (since != 0 && static_cast<int32_t>(mod.lastUpdate - since) <= 0) { return false; }

    JsonObject module = root["module"].to<JsonObject>();
    module["moduleNumber"] = static_cast<int>(index + 1);
    if (!mod.fwVersion.isEmpty()) { module["swversion"] = mod.fwVersion; }
    module["nCells"] = mod.cellCount;

    uint8_t minCell = 0, maxCell = 0, minTemp = 0, maxTemp = 0;
    float tempSum = 0;
    for (uint8_t c = 0; c < mod.cellCount; ++c) {
        if (mod.cellMilliVolt[c] < mod.cellMilliVolt[minCell]) { minCell = c; }
        if (mod.cellMilliVolt[c] > mod.cellMilliVolt[maxCell]) { maxCell = c; }
        if (mod.cellTemperature[c] < mod.cellTemperature[minTemp]) { minTemp = c; }
        if (mod.cellTemperature[c] > mod.cellTemperature[maxTemp]) { maxTemp = c; }
        tempSum += mod.cellTemperature[c];
    }

    // temperature, charging blocked, discharging blocked, heating
    auto values = module["values"].to<JsonArray>();
    if (mod.cellCount > 0) { values.add(tempSum / mod.cellCount); } else { values.add(nullptr); }
    // blocking = not enabled (0x40C, inverted); 32-bit so all modules covered.
    // null until the first 0x40C frame, else the all-zero default reads as
    // "blocking" for every module.
    if (_moduleMasksValid) {
        values.add(static_cast<bool>((~_moduleMaskChargeEnable >> index) & 1));
        values.add(static_cast<bool>((~_moduleMaskDischargeEnable >> index) & 1));
    } else {
        values.add(nullptr);
        values.add(nullptr);
    }
    values.add(static_cast<bool>((_moduleMaskHeating >> index) & 1));

    if (mod.cellCount == 0) { return true; }

    auto cellStatus = module["cellStatus"].to<JsonArray>();
    cellStatus.add(mod.cellMilliVolt[minCell] / 1000.0f);
    cellStatus.add(mod.cellMilliVolt[maxCell] / 1000.0f);
    cellStatus.add(mod.cellMilliVolt[maxCell] - mod.cellMilliVolt[minCell]);
    cellStatus.add(mod.cellTemperature[minTemp]);
    cellStatus.add(mod.cellTemperature[maxTemp]);
    cellStatus.add(minCell + 1);
    cellStatus.add(maxCell + 1);
    cellStatus.add(minTemp + 1);
    cellStatus.add(maxTemp + 1);

    // one row per cell: voltage, temperature
    auto cells = module["cells"].to<JsonArray>();
    for (uint8_t c = 0; c < mod.cellCount; ++c) {
        auto row = cells.add<JsonArray>();
        row.add(mod.cellMilliVolt[c] / 1000.0f);
        row.add(mod.cellTemperature[c]);
    }

    return true;
}

void Stats::mqttPublish() const
{
    ::Batteries::Stats::mqttPublish();

    MqttSettings.publish("battery/settings/chargeVoltage", String(_chargeVoltageLimit));
    MqttSettings.publish("battery/settings/dischargeVoltageLimitation", String(_dischargeVoltageLimit));

    MqttSettings.publish("battery/stateOfHealth", String(_stateOfHealth));
    if (_chargeCycles != -1) {
        MqttSettings.publish("battery/chargeCycles", String(_chargeCycles));
    }
    if (_balance != -1) {
        MqttSettings.publish("battery/balancingActive", String(_balance ? 1 : 0));
    }

    auto oTemperature = getTemperature();
    if (oTemperature) {
        MqttSettings.publish("battery/temperature", String(*oTemperature));
    }

    if (_chargedEnergy != -1) {
        MqttSettings.publish("battery/chargedEnergy", String(_chargedEnergy));
    }

    if (_dischargedEnergy != -1) {
        MqttSettings.publish("battery/dischargedEnergy", String(_dischargedEnergy));
    }

    MqttSettings.publish("battery/capacity", String(_totalCapacity));
    MqttSettings.publish("battery/availableCapacity", String(_availableCapacity));

    MqttSettings.publish("battery/CellMinMilliVolt", String(_cellMinMilliVolt));
    MqttSettings.publish("battery/CellMaxMilliVolt", String(_cellMaxMilliVolt));
    MqttSettings.publish("battery/CellDiffMilliVolt", String(_cellMaxMilliVolt - _cellMinMilliVolt));
    MqttSettings.publish("battery/CellMinTemperature", String(_cellMinTemperature));
    MqttSettings.publish("battery/CellMaxTemperature", String(_cellMaxTemperature));
    MqttSettings.publish("battery/CellMinVoltageName", String(_cellMinVoltageName));
    MqttSettings.publish("battery/CellMaxVoltageName", String(_cellMaxVoltageName));
    MqttSettings.publish("battery/CellMinTemperatureName", String(_cellMinTemperatureName));
    MqttSettings.publish("battery/CellMaxTemperatureName", String(_cellMaxTemperatureName));

    MqttSettings.publish("battery/modulesOnline", String(_moduleCountOnline));
    MqttSettings.publish("battery/modulesOffline", String(_moduleCountOffline));
    MqttSettings.publish("battery/modulesBlockingCharge", String(_moduleCountBlockingCharge));
    MqttSettings.publish("battery/modulesBlockingDischarge", String(_moduleCountBlockingDischarge));
    if (_moduleCountHeating != -1) {
        MqttSettings.publish("battery/modulesHeating", String(_moduleCountHeating));
    }

    MqttSettings.publish("battery/alarm/overCurrentDischarge", String(_alarmOverCurrentDischarge));
    MqttSettings.publish("battery/alarm/overCurrentCharge", String(_alarmOverCurrentCharge));
    MqttSettings.publish("battery/alarm/underVoltage", String(_alarmUnderVoltage));
    MqttSettings.publish("battery/alarm/overVoltage", String(_alarmOverVoltage));
    MqttSettings.publish("battery/alarm/underTemperature", String(_alarmUnderTemperature));
    MqttSettings.publish("battery/alarm/overTemperature", String(_alarmOverTemperature));
    MqttSettings.publish("battery/alarm/underTemperatureCharge", String(_alarmUnderTemperatureCharge));
    MqttSettings.publish("battery/alarm/overTemperatureCharge", String(_alarmOverTemperatureCharge));
    MqttSettings.publish("battery/alarm/bmsInternal", String(_alarmInternalFailure));
    MqttSettings.publish("battery/alarm/cellImbalance", String(_alarmCellImbalance));

    MqttSettings.publish("battery/warning/highCurrentDischarge", String(_warningHighDischargeCurrent));
    MqttSettings.publish("battery/warning/highCurrentCharge", String(_warningHighChargeCurrent));
    MqttSettings.publish("battery/warning/lowVoltage", String(_warningLowVoltage));
    MqttSettings.publish("battery/warning/highVoltage", String(_warningHighVoltage));
    MqttSettings.publish("battery/warning/lowTemperature", String(_warningLowTemperature));
    MqttSettings.publish("battery/warning/highTemperature", String(_warningHighTemperature));
    MqttSettings.publish("battery/warning/lowTemperatureCharge", String(_warningLowTemperatureCharge));
    MqttSettings.publish("battery/warning/highTemperatureCharge", String(_warningHighTemperatureCharge));
    MqttSettings.publish("battery/warning/bmsInternal", String(_warningInternalFailure));
    MqttSettings.publish("battery/warning/cellImbalance", String(_warningCellImbalance));

    mqttPublishModules();

    if (_chargeEnabled) { MqttSettings.publish("battery/charging/chargeEnabled", String(*_chargeEnabled)); }
    if (_dischargeEnabled) { MqttSettings.publish("battery/charging/dischargeEnabled", String(*_dischargeEnabled)); }
    MqttSettings.publish("battery/charging/chargeImmediately", String(_chargeImmediately));
}

// keyed by module number (position on the internal bus of the battery), the
// Pytes CAN protocol has no module serials. value names match the battery
// level topics and those of the Pytes RS485 modules. a module is published
// when new data arrived (each module is requested in turn), and once as
// offline when no data arrived for a minute.
void Stats::mqttPublishModules() const
{
    for (uint8_t i = 0; i < MaxModules; ++i) {
        auto const& mod = _modules[i];
        if (mod.lastUpdate == 0) { continue; }

        String prefix = "battery/module/" + String(i + 1) + "/";
        auto pub = [&prefix](char const* topic, String const& value) {
            MqttSettings.publish(prefix + topic, value);
        };

        if (millis() - mod.lastUpdate > 60 * 1000) {
            if (!mod.offlinePublished) { pub("online", "0"); }
            mod.offlinePublished = true;
            continue;
        }

        if (mod.lastPublish == mod.lastUpdate) { continue; }
        mod.lastPublish = mod.lastUpdate;
        mod.offlinePublished = false;

        pub("online", "1");
        if (!mod.fwVersion.isEmpty()) { pub("fwVersion", mod.fwVersion); }
        pub("cellCount", String(mod.cellCount));

        // blocking = not enabled (0x40C, inverted); 32-bit so all modules.
        // skip until the first 0x40C frame, see getLiveViewModuleData().
        if (_moduleMasksValid) {
            pub("blockingCharge", String((~_moduleMaskChargeEnable >> i) & 1));
            pub("blockingDischarge", String((~_moduleMaskDischargeEnable >> i) & 1));
        }
        pub("heating", String((_moduleMaskHeating >> i) & 1));

        uint8_t minCell = 0, maxCell = 0, minTemp = 0, maxTemp = 0;
        float tempSum = 0;
        for (uint8_t c = 0; c < mod.cellCount; ++c) {
            if (mod.cellMilliVolt[c] < mod.cellMilliVolt[minCell]) { minCell = c; }
            if (mod.cellMilliVolt[c] > mod.cellMilliVolt[maxCell]) { maxCell = c; }
            if (mod.cellTemperature[c] < mod.cellTemperature[minTemp]) { minTemp = c; }
            if (mod.cellTemperature[c] > mod.cellTemperature[maxTemp]) { maxTemp = c; }
            tempSum += mod.cellTemperature[c];

            String cp = "cell/" + String(c + 1) + "/";
            pub((cp + "voltage").c_str(), String(mod.cellMilliVolt[c] / 1000.0f, 3));
            pub((cp + "temperature").c_str(), String(mod.cellTemperature[c], 1));
        }

        if (mod.cellCount == 0) { continue; }

        pub("CellMinMilliVolt", String(mod.cellMilliVolt[minCell]));
        pub("CellMaxMilliVolt", String(mod.cellMilliVolt[maxCell]));
        pub("CellDiffMilliVolt", String(mod.cellMilliVolt[maxCell] - mod.cellMilliVolt[minCell]));
        pub("CellMinVoltageName", String(minCell + 1));
        pub("CellMaxVoltageName", String(maxCell + 1));
        pub("CellMinTemperature", String(mod.cellTemperature[minTemp], 1));
        pub("CellMaxTemperature", String(mod.cellTemperature[maxTemp], 1));
        pub("CellMinTemperatureName", String(minTemp + 1));
        pub("CellMaxTemperatureName", String(maxTemp + 1));
        pub("temperature", String(tempSum / mod.cellCount, 1));
    }
}

} // namespace Batteries::Pytes
