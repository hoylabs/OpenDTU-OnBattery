// SPDX-License-Identifier: GPL-2.0-or-later
#include <MqttSettings.h>
#include <battery/pytes/rs485/Stats.h>

namespace Batteries::Pytes::Rs485 {

using Label = DataPointLabel;

// ---------------------------------------------------------------------------
// Helpers (local to this TU)
// ---------------------------------------------------------------------------

static void addPackValue(JsonObject& obj, char const* key, float v, char const* unit, uint8_t d)
{
    auto f = obj[key].to<JsonObject>();
    f["v"] = v;
    f["u"] = unit;
    f["d"] = d;
}

static void addPackText(JsonObject& obj, char const* key, char const* value, bool translate = true)
{
    auto f = obj[key].to<JsonObject>();
    f["value"] = value;
    f["translate"] = translate;
}

// ---------------------------------------------------------------------------
// updateFrom
// ---------------------------------------------------------------------------
void Stats::updatePackData(DataPointContainer const& dp)
{
    uint32_t ts = millis();

    auto oSoC = dp.get<Label::BatterySoCPercent>();
    if (oSoC.has_value()) {
        uint8_t precision = (*oSoC == static_cast<float>(static_cast<int>(*oSoC))) ? 0 : 2;
        setSoC(*oSoC, precision, ts);
    }

    auto oVolt = dp.get<Label::BatteryVoltageMilliVolt>();
    if (oVolt.has_value()) { setVoltage(*oVolt / 1000.0f, ts); }

    auto oCurr = dp.get<Label::BatteryCurrentMilliAmps>();
    if (oCurr.has_value()) { setCurrent(*oCurr / 1000.0f, 3, ts); }

    auto oChgLim = dp.get<Label::ChargeCurrentLimitMilliAmps>();
    if (oChgLim.has_value()) { setChargeCurrentLimit(*oChgLim / 1000.0f, ts); }

    auto oDsgLim = dp.get<Label::DischargeCurrentLimitMilliAmps>();
    if (oDsgLim.has_value()) { setDischargeCurrentLimit(*oDsgLim / 1000.0f, ts); }

    auto oMfr = dp.get<Label::PackManufacturer>();
    if (oMfr.has_value() && !oMfr->empty()) { setManufacturer(String(oMfr->c_str())); }

    auto oCount = dp.get<Label::ModuleCount>();
    if (oCount.has_value()) {
        resizeModules(*oCount);
    }

    _dataPoints.updateFrom(dp);

    _lastUpdate = ts;
}

// ---------------------------------------------------------------------------
// Module management helpers
// ---------------------------------------------------------------------------
void Stats::resizeModules(size_t n)
{
    if (_modules.size() < n) {
        _modules.resize(n);
    }
}


void Stats::setModuleBasic(uint8_t moduleNo, String hwVersion, String swVersion, String serial)
{
    if (moduleNo == 0) { return; }
    resizeModules(moduleNo);
    auto& mod = _modules[moduleNo - 1];
    mod.hasBasic  = true;
    mod.hwVersion = hwVersion;
    mod.swVersion = swVersion;
    if (mod.serial.isEmpty()) { mod.serial = serial; }
}

void Stats::setModuleAnalog(uint8_t moduleNo, float voltageV, float currentA, float soc, uint16_t health,
                             uint32_t totalCapacityMah, uint32_t remainCapacityMah,
                             float ambientTemp, int chargeCycles, uint16_t balance,
                             float cellMaxV, uint8_t cellMaxNo, float cellMinV, uint8_t cellMinNo,
                             float tempMaxC, uint8_t tempMaxNo, float tempMinC, uint8_t tempMinNo)
{
    if (moduleNo == 0) { return; }
    resizeModules(moduleNo);
    auto& mod = _modules[moduleNo - 1];
    mod.hasAnalog         = true;
    mod.voltageV          = voltageV;
    mod.currentA          = currentA;
    mod.soc               = soc;
    mod.health            = health;
    mod.totalCapacityMah  = totalCapacityMah;
    mod.remainCapacityMah = remainCapacityMah;
    mod.ambientTemp       = ambientTemp;
    mod.chargeCycles      = chargeCycles;
    mod.balance           = balance;
    mod.cellMaxV          = cellMaxV;
    mod.cellMaxNo         = cellMaxNo;
    mod.cellMinV          = cellMinV;
    mod.cellMinNo         = cellMinNo;
    mod.tempMaxC          = tempMaxC;
    mod.tempMaxNo         = tempMaxNo;
    mod.tempMinC          = tempMinC;
    mod.tempMinNo         = tempMinNo;
    mod.hasTempMinMax     = true;

}

void Stats::setModuleChgDsg(uint8_t moduleNo, float maxChgVoltV, float minDsgVoltV,
                              float maxChgCurrA, float maxDsgCurrA, bool fullChgReq, uint8_t emergFlags)
{
    if (moduleNo == 0) { return; }
    resizeModules(moduleNo);
    auto& mod = _modules[moduleNo - 1];
    mod.hasChgDsg  = true;
    mod.maxChgVoltV = maxChgVoltV;
    mod.minDsgVoltV = minDsgVoltV;
    mod.maxChgCurrA = maxChgCurrA;
    mod.maxDsgCurrA = maxDsgCurrA;
    mod.fullChgReq  = fullChgReq;
    mod.emergFlags  = emergFlags;
}

void Stats::setModuleCells(uint8_t moduleNo, std::vector<CellData> cells)
{
    if (moduleNo == 0 || cells.size() == 0) { return; }
    resizeModules(moduleNo);
    auto& mod = _modules[moduleNo - 1];
    mod.hasCells = true;
    mod.nCells   = cells.size();
    mod.cells    = std::move(cells);

    // update average cell temperature across all modules that have cell data
    float sumTemp = 0.0f;
    int count = 0;
    for (auto const& m : _modules) {
        for (auto const& c : m.cells) {
            sumTemp += c.temperatureC;
            ++count;
        }
    }
    if (count > 0) { setTemperature(sumTemp / count, millis()); }
}

void Stats::setModuleProtect(uint8_t moduleNo, ProtectParams const& protect)
{
    if (moduleNo == 0) { return; }
    resizeModules(moduleNo);
    auto& mod = _modules[moduleNo - 1];
    mod.hasProtect  = true;
    mod.protect     = protect;
}

// ---------------------------------------------------------------------------
// getImmediateChargingRequest
// ---------------------------------------------------------------------------
bool Stats::getImmediateChargingRequest() const
{
    auto o = _dataPoints.get<Label::ChargeImmediately>();
    return o.has_value() && *o;
}

// ---------------------------------------------------------------------------
// getLiveViewData
// ---------------------------------------------------------------------------
void Stats::getLiveViewData(JsonVariant& root) const
{
    ::Batteries::Stats::getLiveViewData(root);

    auto oChgVolt = _dataPoints.get<Label::ChargeVoltageLimitMilliVolt>();
    if (oChgVolt.has_value()) {
        addLiveViewValue(root, "chargeVoltage", *oChgVolt / 1000.0f, "V", 1);
    }
    auto oDsgVolt = _dataPoints.get<Label::DischargeVoltageLimitMilliVolt>();
    if (oDsgVolt.has_value()) {
        addLiveViewValue(root, "dischargeVoltageLimitation", *oDsgVolt / 1000.0f, "V", 1);
    }

    auto oSoH = _dataPoints.get<Label::BatterySoHPercent>();
    if (oSoH.has_value()) {
        addLiveViewValue(root, "stateOfHealth", static_cast<uint32_t>(*oSoH), "%", 0);
    }

    auto oTemperature = getTemperature();
    if (oTemperature) {
        addLiveViewValue(root, "temperature", *oTemperature, "°C", 1);
    }

    auto oTotal = _dataPoints.get<Label::TotalCapacityMilliAmpHours>();
    auto oRemain = _dataPoints.get<Label::RemainingCapacityMilliAmpHours>();
    if (oTotal.has_value()) {
        addLiveViewValue(root, "capacity", *oTotal / 1000.0f, "Ah", 2);
    }
    if (oRemain.has_value()) {
        addLiveViewValue(root, "availableCapacity", *oRemain / 1000.0f, "Ah", 2);
    }

    auto oChg = _dataPoints.get<Label::AccumulatedChargeDeciKWh>();
    if (oChg.has_value()) {
        addLiveViewValue(root, "chargedEnergy", *oChg * 0.1f, "kWh", 1);
    }
    auto oDsg = _dataPoints.get<Label::AccumulatedDischargeDeciKWh>();
    if (oDsg.has_value()) {
        addLiveViewValue(root, "dischargedEnergy", *oDsg * 0.1f, "kWh", 1);
    }

    auto oImm = _dataPoints.get<Label::ChargeImmediately>();
    addLiveViewTextValue(root, "chargeImmediately", (oImm.has_value() && *oImm) ? "yes" : "no");

    auto oCellMin = _dataPoints.get<Label::CellMinMilliVolt>();
    auto oCellMax = _dataPoints.get<Label::CellMaxMilliVolt>();
    if (oCellMin.has_value()) {
        addLiveViewInSection(root, "cells", "cellMinVoltage",
            static_cast<float>(*oCellMin) / 1000.0f, "V", 3);
    }
    if (oCellMax.has_value()) {
        addLiveViewInSection(root, "cells", "cellMaxVoltage",
            static_cast<float>(*oCellMax) / 1000.0f, "V", 3);
    }
    if (oCellMin.has_value() && oCellMax.has_value()) {
        addLiveViewInSection(root, "cells", "cellDiffVoltage",
            static_cast<int>(*oCellMax - *oCellMin), "mV", 0);
    }

    auto oTempMin = _dataPoints.get<Label::CellMinTemperatureCelsius>();
    auto oTempMax = _dataPoints.get<Label::CellMaxTemperatureCelsius>();
    if (oTempMin.has_value()) {
        addLiveViewInSection(root, "cells", "cellMinTemperature",
            static_cast<float>(*oTempMin), "°C", 0);
    }
    if (oTempMax.has_value()) {
        addLiveViewInSection(root, "cells", "cellMaxTemperature",
            static_cast<float>(*oTempMax), "°C", 0);
    }

    // Alarms
    auto oAlarms = _dataPoints.get<Label::AlarmsBitmask>();
    if (oAlarms.has_value()) {
        uint16_t alarms = *oAlarms;
#define ALARM(name, value) addLiveViewAlarm(root, #name, (alarms & static_cast<uint16_t>(AlarmBits::name)) != 0);
        PYTESRS485_ALARM_BITS(ALARM)
#undef ALARM
    }

    // Warnings
    auto oWarnings = _dataPoints.get<Label::WarningsBitmask>();
    if (oWarnings.has_value()) {
        uint16_t warnings = *oWarnings;
#define WARNING(name, value) addLiveViewWarning(root, #name, (warnings & static_cast<uint16_t>(WarningBits::name)) != 0);
        PYTESRS485_WARNING_BITS(WARNING)
#undef WARNING
    }

    root["numberOfModules"] = static_cast<int>(_modules.size());

    JsonArray modules = root["modules"].to<JsonArray>();
    for (size_t i = 0; i < _modules.size(); ++i) {
        auto const& mod = _modules[i];
        JsonObject module = modules.add<JsonObject>();

        module["moduleNumber"] = static_cast<int>(i + 1);
        module["moduleName"]   = String("Module ") + static_cast<int>(i + 1);
        module["moduleSerialNumber"] = mod.serial;

        if (mod.hasBasic) {
            if (!mod.hwVersion.isEmpty()) { module["moduleName"] = String(static_cast<int>(i + 1)) + " - " + mod.hwVersion; }
            if (!mod.swVersion.isEmpty()) { module["swversion"] = mod.swVersion; }
            if (mod.nCells > 0)           { module["nCells"]    = mod.nCells; }
        }

        auto modValues = module["values"].to<JsonObject>();

        if (mod.hasAnalog) {
            uint8_t socPrecision = (mod.soc == static_cast<float>(static_cast<int>(mod.soc))) ? 0 : 2;
            addPackValue(modValues, "SoC", mod.soc, "%", socPrecision);

            addPackValue(modValues, "voltage",      mod.voltageV,  "V",   3);
            addPackValue(modValues, "current",      mod.currentA,  "A",   3);

            if (mod.hasChgDsg) {
                addPackValue(modValues, "chargeCurrentLimitation", mod.maxChgCurrA, "A", 1);
                addPackValue(modValues, "dischargeCurrentLimitation", mod.maxDsgCurrA, "A", 1);
                addPackValue(modValues, "chargeVoltage", mod.maxChgVoltV, "V", 1);
                addPackValue(modValues, "dischargeVoltageLimitation", mod.minDsgVoltV, "V", 1);
            }

            if (mod.health > 0) {
                addPackValue(modValues, "stateOfHealth", static_cast<float>(mod.health), "%", 0);
            }

            if (mod.chargeCycles >= 0) {
                addPackValue(modValues, "chargeCycles", static_cast<float>(mod.chargeCycles), "", 0);
            }

            addPackValue(modValues, "temperature",  mod.ambientTemp, "°C", 1);

            if (mod.totalCapacityMah > 0) {
                addPackValue(modValues, "capacity", mod.totalCapacityMah  / 1000.0f, "Ah", 2);
            }

            if (mod.remainCapacityMah > 0) {
                addPackValue(modValues, "availableCapacity", mod.remainCapacityMah / 1000.0f, "Ah", 2);
            }

            if (mod.hasChgDsg) {
                addPackText(modValues, "chargeImmediately", mod.fullChgReq ? "yes" : "no");
            }

            addPackText(modValues, "balancingActive", mod.balance != 0 ? "yes" : "no");

            auto cellStatus = module["cellStatus"].to<JsonObject>();

            auto csMinV = cellStatus["cellMinVoltage"].to<JsonObject>();
            csMinV["v"] = mod.cellMinV;
            csMinV["u"] = "V";
            csMinV["d"] = 3;

            auto csMaxV = cellStatus["cellMaxVoltage"].to<JsonObject>();
            csMaxV["v"] = mod.cellMaxV;
            csMaxV["u"] = "V";
            csMaxV["d"] = 3;

            auto csDiff = cellStatus["cellDiffVoltage"].to<JsonObject>();
            csDiff["v"] = static_cast<int>((mod.cellMaxV - mod.cellMinV) * 1000.0f + 0.5f);
            csDiff["u"] = "mV";
            csDiff["d"] = 0;

            auto csMinT = cellStatus["cellMinTemperature"].to<JsonObject>();
            csMinT["v"] = mod.tempMinC;
            csMinT["u"] = "°C";
            csMinT["d"] = 1;

            auto csMaxT = cellStatus["cellMaxTemperature"].to<JsonObject>();
            csMaxT["v"] = mod.tempMaxC;
            csMaxT["u"] = "°C";
            csMaxT["d"] = 1;

            auto csMinVno = cellStatus["cellMinVoltageName"].to<JsonObject>();
            csMinVno["v"] = mod.cellMinNo;
            csMinVno["u"] = "";
            csMinVno["d"] = 0;

            auto csMaxVno = cellStatus["cellMaxVoltageName"].to<JsonObject>();
            csMaxVno["v"] = mod.cellMaxNo;
            csMaxVno["u"] = "";
            csMaxVno["d"] = 0;

            auto csMinTno = cellStatus["cellMinTemperatureName"].to<JsonObject>();
            csMinTno["v"] = mod.tempMinNo;
            csMinTno["u"] = "";
            csMinTno["d"] = 0;

            auto csMaxTno = cellStatus["cellMaxTemperatureName"].to<JsonObject>();
            csMaxTno["v"] = mod.tempMaxNo;
            csMaxTno["u"] = "";
            csMaxTno["d"] = 0;
        }

        if (mod.hasCells && !mod.cells.empty()) {
            auto cells = module["cells"].to<JsonArray>();
            for (auto const& c : mod.cells) {
                auto cell = cells.add<JsonObject>();

                auto cv = cell["voltage"].to<JsonObject>();
                cv["v"] = c.voltageV;
                cv["u"] = "V";
                cv["d"] = 3;

                auto ct = cell["temperature"].to<JsonObject>();
                ct["v"] = c.temperatureC;
                ct["u"] = "°C";
                ct["d"] = 1;
            }
        }

        if (mod.hasProtect) {
            auto p = module["parameters"].to<JsonObject>();
            auto const& pr = mod.protect;
            addPackValue(p, "overvoltageProtection",      static_cast<float>(pr.overvoltageProtectionMv),  "mV", 0);
            addPackValue(p, "undervoltageProtection",     static_cast<float>(pr.undervoltageProtectionMv), "mV", 0);
            addPackValue(p, "highVoltageAlarm",           static_cast<float>(pr.highVoltageAlarmMv),       "mV", 0);
            addPackValue(p, "lowVoltageAlarm",            static_cast<float>(pr.lowVoltageAlarmMv),        "mV", 0);
            addPackValue(p, "chargeOverTempProtection",   pr.chargeOverTempProtMc   / 1000.0f, "°C", 1);
            addPackValue(p, "chargeUnderTempProtection",  pr.chargeUnderTempProtMc  / 1000.0f, "°C", 1);
            addPackValue(p, "chargeHighTempAlarm",        pr.chargeHighTempAlarmMc  / 1000.0f, "°C", 1);
            addPackValue(p, "chargeLowTempAlarm",         pr.chargeLowTempAlarmMc   / 1000.0f, "°C", 1);
            addPackValue(p, "dischargeOverTempProtection",  pr.dischargeOverTempProtMc  / 1000.0f, "°C", 1);
            addPackValue(p, "dischargeUnderTempProtection", pr.dischargeUnderTempProtMc / 1000.0f, "°C", 1);
            addPackValue(p, "dischargeHighTempAlarm",       pr.dischargeHighTempAlarmMc / 1000.0f, "°C", 1);
            addPackValue(p, "dischargeLowTempAlarm",        pr.dischargeLowTempAlarmMc  / 1000.0f, "°C", 1);
            addPackValue(p, "chargeOvercurrent",          pr.chargeOvercurrentMa    / 1000.0f, "A", 1);
            addPackValue(p, "dischargeOvercurrent",       pr.dischargeOvercurrentMa / 1000.0f, "A", 1);
            addPackValue(p, "balanceStartVoltage",        static_cast<float>(pr.balanceStartVoltageMv), "mV", 0);
            addPackValue(p, "balanceDiff",                static_cast<float>(pr.balanceDiffMv),         "mV", 0);
        }
    }
}

// ---------------------------------------------------------------------------
// mqttPublish
// ---------------------------------------------------------------------------
void Stats::mqttPublish() const
{
    ::Batteries::Stats::mqttPublish();

    auto oChgVolt = _dataPoints.get<Label::ChargeVoltageLimitMilliVolt>();
    if (oChgVolt.has_value()) {
        MqttSettings.publish("battery/settings/chargeVoltage",
            String(*oChgVolt / 1000.0f, 3));
    }
    auto oDsgVolt = _dataPoints.get<Label::DischargeVoltageLimitMilliVolt>();
    if (oDsgVolt.has_value()) {
        MqttSettings.publish("battery/settings/dischargeVoltageLimitation",
            String(*oDsgVolt / 1000.0f, 3));
    }

    auto oSoH = _dataPoints.get<Label::BatterySoHPercent>();
    if (oSoH.has_value()) {
        MqttSettings.publish("battery/stateOfHealth", String(*oSoH));
    }

    auto oTemperature = getTemperature();
    if (oTemperature) {
        MqttSettings.publish("battery/temperature", String(*oTemperature));
    }

    auto oChg = _dataPoints.get<Label::AccumulatedChargeDeciKWh>();
    if (oChg.has_value()) {
        MqttSettings.publish("battery/chargedEnergy", String(*oChg * 0.1f, 1));
    }
    auto oDsg = _dataPoints.get<Label::AccumulatedDischargeDeciKWh>();
    if (oDsg.has_value()) {
        MqttSettings.publish("battery/dischargedEnergy", String(*oDsg * 0.1f, 1));
    }

    auto oTotal = _dataPoints.get<Label::TotalCapacityMilliAmpHours>();
    if (oTotal.has_value()) {
        MqttSettings.publish("battery/capacity", String(*oTotal / 1000.0f, 2));
    }
    auto oRemain = _dataPoints.get<Label::RemainingCapacityMilliAmpHours>();
    if (oRemain.has_value()) {
        MqttSettings.publish("battery/availableCapacity", String(*oRemain / 1000.0f, 2));
    }

    auto oCellMin = _dataPoints.get<Label::CellMinMilliVolt>();
    auto oCellMax = _dataPoints.get<Label::CellMaxMilliVolt>();
    if (oCellMin.has_value()) {
        MqttSettings.publish("battery/CellMinMilliVolt", String(*oCellMin));
    }
    if (oCellMax.has_value()) {
        MqttSettings.publish("battery/CellMaxMilliVolt", String(*oCellMax));
    }
    if (oCellMin.has_value() && oCellMax.has_value()) {
        MqttSettings.publish("battery/CellDiffMilliVolt", String(*oCellMax - *oCellMin));
    }

    auto oTempMin = _dataPoints.get<Label::CellMinTemperatureCelsius>();
    auto oTempMax = _dataPoints.get<Label::CellMaxTemperatureCelsius>();
    if (oTempMin.has_value()) {
        MqttSettings.publish("battery/CellMinTemperature", String(*oTempMin));
    }
    if (oTempMax.has_value()) {
        MqttSettings.publish("battery/CellMaxTemperature", String(*oTempMax));
    }

    // Alarms
    auto oAlarms = _dataPoints.get<Label::AlarmsBitmask>();
    if (oAlarms.has_value()) {
        uint16_t alarms = *oAlarms;
#define ALARM(name, value) MqttSettings.publish("battery/alarm/" #name, String((alarms & static_cast<uint16_t>(AlarmBits::name)) != 0));
        PYTESRS485_ALARM_BITS(ALARM)
#undef ALARM
    }

    // Warnings
    auto oWarnings = _dataPoints.get<Label::WarningsBitmask>();
    if (oWarnings.has_value()) {
        uint16_t warnings = *oWarnings;
#define WARNING(name, value) MqttSettings.publish("battery/warning/" #name, String((warnings & static_cast<uint16_t>(WarningBits::name)) != 0));
        PYTESRS485_WARNING_BITS(WARNING)
#undef WARNING
    }

    auto oImm = _dataPoints.get<Label::ChargeImmediately>();
    if (oImm.has_value()) {
        MqttSettings.publish("battery/charging/chargeImmediately", String(*oImm ? 1 : 0));
    }

    // Per-module data
    for (size_t i = 0; i < _modules.size(); ++i) {
        auto const& mod = _modules[i];
        String prefix = "battery/module/" + String(static_cast<int>(i + 1)) + "/";

        if (mod.hasAnalog) {
            MqttSettings.publish(prefix + "voltage",           String(mod.voltageV, 3));
            MqttSettings.publish(prefix + "current",           String(mod.currentA, 3));
            MqttSettings.publish(prefix + "stateOfCharge",     String(mod.soc));
            if (mod.health > 0) {
                MqttSettings.publish(prefix + "stateOfHealth", String(mod.health));
            }
            if (mod.totalCapacityMah > 0) {
                MqttSettings.publish(prefix + "capacity",          String(mod.totalCapacityMah  / 1000.0f, 2));
                MqttSettings.publish(prefix + "availableCapacity", String(mod.remainCapacityMah / 1000.0f, 2));
            }
            MqttSettings.publish(prefix + "temperature",        String(mod.ambientTemp, 1));
            if (mod.chargeCycles >= 0) {
                MqttSettings.publish(prefix + "chargeCycles",  String(mod.chargeCycles));
            }
            MqttSettings.publish(prefix + "balancingActive",   String(mod.balance != 0 ? 1 : 0));
        }

        if (mod.hasChgDsg) {
            MqttSettings.publish(prefix + "settings/chargeVoltage",                String(mod.maxChgVoltV, 3));
            MqttSettings.publish(prefix + "settings/dischargeVoltageLimitation",   String(mod.minDsgVoltV, 3));
            MqttSettings.publish(prefix + "settings/chargeCurrentLimitation",      String(mod.maxChgCurrA, 3));
            MqttSettings.publish(prefix + "settings/dischargeCurrentLimitation",   String(mod.maxDsgCurrA, 3));
            MqttSettings.publish(prefix + "charging/chargeImmediately",            String(mod.fullChgReq ? 1 : 0));
        }

        for (size_t j = 0; j < mod.cells.size(); ++j) {
            String cp = prefix + "cell/" + String(static_cast<int>(j + 1)) + "/";
            MqttSettings.publish(cp + "voltage",     String(mod.cells[j].voltageV, 3));
            MqttSettings.publish(cp + "temperature", String(mod.cells[j].temperatureC, 1));
        }
    }
}

} // namespace Batteries::Pytes::Rs485
