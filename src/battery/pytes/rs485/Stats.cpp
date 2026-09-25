// SPDX-License-Identifier: GPL-2.0-or-later
#include <Configuration.h>
#include <MqttSettings.h>
#include <battery/pytes/rs485/Stats.h>

namespace Batteries::Pytes::Rs485 {

using Label = DataPointLabel;

// ---------------------------------------------------------------------------
// Helpers (local to this TU)
// ---------------------------------------------------------------------------

static void addValue(JsonObject& obj, char const* key, float v, char const* unit, uint8_t d)
{
    auto f = obj[key].to<JsonObject>();
    f["v"] = v;
    f["u"] = unit;
    f["d"] = d;
}

static void addText(JsonObject& obj, char const* key, char const* value, bool translate = true)
{
    auto f = obj[key].to<JsonObject>();
    f["value"] = value;
    f["translate"] = translate;
}

// ---------------------------------------------------------------------------
// updateFrom
// ---------------------------------------------------------------------------
void Stats::updateBatteryData(DataPointContainer const& dp)
{
    std::lock_guard<std::mutex> lock(_mutex);
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

    auto oMfr = dp.get<Label::Manufacturer>();
    if (oMfr.has_value() && !oMfr->empty()) { setManufacturer(String(oMfr->c_str())); }

    auto oCount = dp.get<Label::ModuleCount>();
    if (oCount.has_value()) {
        _modules.resize(*oCount); // also drop modules no longer reported
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


void Stats::setModuleBasic(Parsers::ModuleBasicResult const& r)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (r.moduleNo == 0) { return; }
    resizeModules(r.moduleNo);
    auto& mod = _modules[r.moduleNo - 1];
    mod.hasBasic  = true;
    mod.hwVersion = r.hwVersion;
    mod.swVersion = r.swVersion;
    mod.serial    = r.serial; // re-read after a module count change, the module may have been swapped
    if (r.nCells > 0 && r.nCells != 0xFF) { mod.nCells = r.nCells; }
}

void Stats::setModuleAnalog(Parsers::ModuleAnalogResult const& r)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (r.moduleNo == 0) { return; }
    resizeModules(r.moduleNo);
    auto& mod = _modules[r.moduleNo - 1];
    mod.hasAnalog         = true;
    mod.lastUpdate        = millis();
    mod.voltageV          = r.voltageV;
    mod.currentA          = r.currentA;
    mod.soc               = r.soc;
    mod.health            = r.health;
    mod.totalCapacityMah  = r.totalCapacityMah;
    mod.remainCapacityMah = r.remainCapacityMah;
    mod.ambientTemp       = r.ambientTemp;
    mod.chargeCycles      = r.chargeCycles;
    mod.balance           = r.balance;
    mod.status            = r.status;
    mod.errorStatus       = r.errorStatus;
    mod.cellMaxV          = r.cellMaxV;
    mod.cellMaxNo         = r.cellMaxNo;
    mod.cellMinV          = r.cellMinV;
    mod.cellMinNo         = r.cellMinNo;
    mod.tempMaxC          = r.tempMaxC;
    mod.tempMaxNo         = r.tempMaxNo;
    mod.tempMinC          = r.tempMinC;
    mod.tempMinNo         = r.tempMinNo;
    mod.hasTempMinMax     = true;
}

void Stats::setModuleChgDsg(Parsers::ModuleChgDsgResult const& r)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (r.moduleNo == 0) { return; }
    resizeModules(r.moduleNo);
    auto& mod = _modules[r.moduleNo - 1];
    mod.hasChgDsg   = true;
    mod.maxChgVoltV = r.maxChgVoltV;
    mod.minDsgVoltV = r.minDsgVoltV;
    mod.maxChgCurrA = r.maxChgCurrA;
    mod.maxDsgCurrA = r.maxDsgCurrA;
    mod.chargeImmediately = r.chargeImmediately;
    mod.fullChgReq  = r.fullChgReq;
}

void Stats::setModuleCells(uint8_t moduleNo, std::vector<CellData> cells)
{
    std::lock_guard<std::mutex> lock(_mutex);
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
        if (!isOnline(m)) { continue; }
        for (auto const& c : m.cells) {
            sumTemp += c.temperatureC;
            ++count;
        }
    }
    if (count > 0) { setTemperature(sumTemp / count, millis()); }
}

String Stats::moduleName(size_t index) const
{
    auto const& mod = _modules[index];
    String number(static_cast<int>(index + 1));
    if (mod.hasBasic && !mod.hwVersion.isEmpty()) { return number + " - " + mod.hwVersion; }
    return "Module " + number;
}

// serials are used as MQTT topic level, only accept what the spec allows
// (A-Z, a-z, 0-9, '.'), so nothing can break the topic structure
static bool isUsableSerial(String const& serial)
{
    if (serial.isEmpty()) { return false; }
    for (char c : serial) {
        if (!isalnum(static_cast<unsigned char>(c)) && c != '.') { return false; }
    }
    return true;
}

std::vector<Stats::ModuleIdentity> Stats::getModuleIdentities() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<ModuleIdentity> ids;
    for (auto const& m : _modules) {
        if (isUsableSerial(m.serial)) { ids.push_back({ m.serial, m.hwVersion, m.swVersion, m.nCells }); }
    }
    return ids;
}

// a module is offline if it did not answer the analog query (0x81) for three
// poll cycles (plus some slack for the time a cycle takes with many modules)
bool Stats::isOnline(BatteryModule const& mod)
{
    if (!mod.hasAnalog) { return false; }
    uint32_t pollIntervalMs = Configuration.get().Battery.Serial.PollingInterval * 1000;
    return millis() - mod.lastUpdate < 3 * pollIntervalMs + 10 * 1000;
}

// "equilibrium state" (0x81): 0 = not balancing. Any other value means balancing,
// likely a per-cell bitmask (not verified yet). 0xFFFF = unsupported (spec 1.3.5).
std::optional<bool> Stats::isBalancing(BatteryModule const& mod)
{
    if (!mod.hasAnalog || mod.balance == 0xFFFF) { return std::nullopt; }
    return mod.balance != 0;
}

std::optional<float> Stats::averageCellTemperature(BatteryModule const& mod)
{
    if (mod.cells.empty()) { return std::nullopt; }
    float sum = 0.0f;
    for (auto const& c : mod.cells) { sum += c.temperatureC; }
    return sum / mod.cells.size();
}

// battery-level values not provided by 0x61, derived from the per-module data

// same meaning as the Pytes CAN provider: online modules refusing charge/discharge
Stats::ModuleCounts Stats::getModuleCounts() const
{
    ModuleCounts counts;
    for (auto const& m : _modules) {
        if (!isOnline(m) || !m.hasChgDsg) { continue; }
        if (m.maxChgCurrA <= 0.0f) { ++counts.blockingCharge; }
        if (m.maxDsgCurrA <= 0.0f) { ++counts.blockingDischarge; }
    }
    return counts;
}

std::optional<bool> Stats::isBalancing() const
{
    std::optional<bool> res;
    for (auto const& m : _modules) {
        auto oBalancing = isBalancing(m);
        if (isOnline(m) && oBalancing) { res = res.value_or(false) || *oBalancing; }
    }
    return res;
}

// "<module>-<cell>" of the module holding the battery-wide min/max cell value
std::optional<std::string> Stats::extremeCellName(float BatteryModule::*value, uint8_t BatteryModule::*no, bool max) const
{
    std::optional<size_t> idx;
    for (size_t i = 0; i < _modules.size(); ++i) {
        auto const& m = _modules[i];
        if (!isOnline(m)) { continue; }
        if (!idx || (max ? m.*value > _modules[*idx].*value : m.*value < _modules[*idx].*value)) { idx = i; }
    }
    if (!idx) { return std::nullopt; }
    return std::to_string(*idx + 1) + "-" + std::to_string(_modules[*idx].*no);
}

// ---------------------------------------------------------------------------
// getImmediateChargingRequest
// ---------------------------------------------------------------------------
bool Stats::getImmediateChargingRequest() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto o = _dataPoints.get<Label::ChargeImmediately>();
    return o.has_value() && *o;
}

// ---------------------------------------------------------------------------
// getLiveViewData
// ---------------------------------------------------------------------------
void Stats::getLiveViewData(JsonVariant& root) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    ::Batteries::Stats::getLiveViewData(root);

    // sections and their order mirror the per-module cards below

    // status: the base class added SoC, voltage and current (and the current
    // limits, which are moved to "limits" below to keep a fixed order there)
    auto status = root["values"]["status"];
    auto chgCurrLimit = status["chargeCurrentLimitation"];
    auto dsgCurrLimit = status["dischargeCurrentLimitation"];

    auto oSoH = _dataPoints.get<Label::BatterySoHPercent>();
    if (oSoH.has_value()) {
        addLiveViewValue(root, "stateOfHealth", static_cast<uint32_t>(*oSoH), "%", 0);
    }

    auto oTemperature = getTemperature();
    if (oTemperature) {
        addLiveViewValue(root, "temperature", *oTemperature, "°C", 1);
    }

    auto oBalancing = isBalancing();
    if (oBalancing) { addLiveViewTextValue(root, "balancingActive", *oBalancing ? "yes" : "no"); }

    auto oImm = _dataPoints.get<Label::ChargeImmediately>();
    addLiveViewTextValue(root, "chargeImmediately", (oImm.has_value() && *oImm) ? "yes" : "no");

    auto oFullChgReq = _dataPoints.get<Label::FullChargeRequest>();
    if (oFullChgReq.has_value()) { addLiveViewTextValue(root, "fullChargeRequest", *oFullChgReq ? "yes" : "no"); }

    // limits
    auto oChgVolt = _dataPoints.get<Label::ChargeVoltageLimitMilliVolt>();
    if (oChgVolt.has_value()) {
        addLiveViewInSection(root, "limits", "chargeVoltage", *oChgVolt / 1000.0f, "V", 1);
    }
    auto oDsgVolt = _dataPoints.get<Label::DischargeVoltageLimitMilliVolt>();
    if (oDsgVolt.has_value()) {
        addLiveViewInSection(root, "limits", "dischargeVoltageLimitation", *oDsgVolt / 1000.0f, "V", 1);
    }
    if (!chgCurrLimit.isNull()) { root["values"]["limits"]["chargeCurrentLimitation"] = chgCurrLimit; }
    if (!dsgCurrLimit.isNull()) { root["values"]["limits"]["dischargeCurrentLimitation"] = dsgCurrLimit; }
    status.remove("chargeCurrentLimitation");
    status.remove("dischargeCurrentLimitation");

    // capacities
    auto oTotal = _dataPoints.get<Label::TotalCapacityMilliAmpHours>();
    if (oTotal.has_value()) {
        addLiveViewInSection(root, "capacities", "capacity", *oTotal / 1000.0f, "Ah", 2);
    }
    auto oRemain = _dataPoints.get<Label::RemainingCapacityMilliAmpHours>();
    if (oRemain.has_value()) {
        addLiveViewInSection(root, "capacities", "availableCapacity", *oRemain / 1000.0f, "Ah", 2);
    }
    auto oChg = _dataPoints.get<Label::AccumulatedChargeDeciKWh>();
    if (oChg.has_value()) {
        addLiveViewInSection(root, "capacities", "chargedEnergy", *oChg * 0.1f, "kWh", 1);
    }
    auto oDsg = _dataPoints.get<Label::AccumulatedDischargeDeciKWh>();
    if (oDsg.has_value()) {
        addLiveViewInSection(root, "capacities", "dischargedEnergy", *oDsg * 0.1f, "kWh", 1);
    }

    // cell_status
    auto oCellMin = _dataPoints.get<Label::CellMinMilliVolt>();
    auto oCellMax = _dataPoints.get<Label::CellMaxMilliVolt>();
    if (oCellMin.has_value()) {
        addLiveViewInSection(root, "cell_status", "cellMinVoltage",
            static_cast<float>(*oCellMin) / 1000.0f, "V", 3);
    }
    if (oCellMax.has_value()) {
        addLiveViewInSection(root, "cell_status", "cellMaxVoltage",
            static_cast<float>(*oCellMax) / 1000.0f, "V", 3);
    }
    if (oCellMin.has_value() && oCellMax.has_value()) {
        addLiveViewInSection(root, "cell_status", "cellDiffVoltage",
            static_cast<int>(*oCellMax - *oCellMin), "mV", 0);
    }

    auto oTempMin = _dataPoints.get<Label::CellMinTemperatureCelsius>();
    auto oTempMax = _dataPoints.get<Label::CellMaxTemperatureCelsius>();
    if (oTempMin.has_value()) {
        addLiveViewInSection(root, "cell_status", "cellMinTemperature", *oTempMin, "°C", 1);
    }
    if (oTempMax.has_value()) {
        addLiveViewInSection(root, "cell_status", "cellMaxTemperature", *oTempMax, "°C", 1);
    }

    auto addCellName = [&](char const* key, std::optional<std::string> const& o) {
        if (o) { addLiveViewTextInSection(root, "cell_status", key, *o, false); }
    };
    addCellName("cellMinVoltageName", extremeCellName(&BatteryModule::cellMinV, &BatteryModule::cellMinNo, false));
    addCellName("cellMaxVoltageName", extremeCellName(&BatteryModule::cellMaxV, &BatteryModule::cellMaxNo, true));
    addCellName("cellMinTemperatureName", extremeCellName(&BatteryModule::tempMinC, &BatteryModule::tempMinNo, false));
    addCellName("cellMaxTemperatureName", extremeCellName(&BatteryModule::tempMaxC, &BatteryModule::tempMaxNo, true));

    // same "Battery modules" card as the Pytes CAN provider
    auto counts = getModuleCounts();
    addLiveViewInSection(root, "modules", "blockingCharge", counts.blockingCharge, "", 0);
    addLiveViewInSection(root, "modules", "blockingDischarge", counts.blockingDischarge, "", 0);

    // Alarms
    auto oAlarms = _dataPoints.get<Label::AlarmsBitmask>();
    if (oAlarms.has_value()) {
        uint16_t alarms = *oAlarms;
#define ALARM(name, value, key) addLiveViewAlarm(root, key, (alarms & static_cast<uint16_t>(AlarmBits::name)) != 0);
        PYTESRS485_ALARM_BITS(ALARM)
#undef ALARM
    }

    // Warnings
    auto oWarnings = _dataPoints.get<Label::WarningsBitmask>();
    if (oWarnings.has_value()) {
        uint16_t warnings = *oWarnings;
#define WARNING(name, value, key) addLiveViewWarning(root, key, (warnings & static_cast<uint16_t>(WarningBits::name)) != 0);
        PYTESRS485_WARNING_BITS(WARNING)
#undef WARNING
    }

    root["numberOfModules"] = static_cast<int>(_modules.size());

    JsonArray modules = root["modules"].to<JsonArray>();
    for (size_t i = 0; i < _modules.size(); ++i) {
        auto const& mod = _modules[i];
        JsonObject module = modules.add<JsonObject>();

        module["moduleNumber"] = static_cast<int>(i + 1);
        module["moduleName"]   = moduleName(i);
        module["moduleSerialNumber"] = mod.serial;

        if (mod.hasBasic) {
            if (!mod.swVersion.isEmpty()) { module["swversion"] = mod.swVersion; }
            if (mod.nCells > 0)           { module["nCells"]    = mod.nCells; }
        }

        // only identify offline modules, their last values are outdated
        module["online"] = isOnline(mod);
        if (!isOnline(mod)) { continue; }

        // sections and their order mirror the battery cards above
        // system error bits (spec 1.3.8) or FAULT status flag (spec 1.3.7, bit 28)
        if (mod.hasAnalog && (mod.errorStatus != 0 || (mod.status & (1u << 28)) != 0)) {
            char buf[11];
            snprintf(buf, sizeof(buf), "0x%08X", static_cast<unsigned>(mod.errorStatus));
            module["error"] = buf; // ponytail: raw bitmask, decode when LV/HV variant is known
        }

        auto modValues = module["values"].to<JsonObject>(); // status

        if (mod.hasAnalog) {
            uint8_t socPrecision = (mod.soc == static_cast<float>(static_cast<int>(mod.soc))) ? 0 : 2;
            addValue(modValues, "SoC", mod.soc, "%", socPrecision);
            addValue(modValues, "voltage", mod.voltageV, "V", 2);
            addValue(modValues, "current", mod.currentA, "A", 3);
            if (mod.health > 0) {
                addValue(modValues, "stateOfHealth", static_cast<float>(mod.health), "%", 0);
            }
        }

        auto oModTemp = averageCellTemperature(mod);
        if (oModTemp) { addValue(modValues, "temperature", *oModTemp, "°C", 1); }

        if (mod.hasAnalog) {
            auto oBalancing = isBalancing(mod);
            if (oBalancing) { addText(modValues, "balancingActive", *oBalancing ? "yes" : "no"); }
        }
        if (mod.hasChgDsg) {
            addText(modValues, "chargeImmediately", mod.chargeImmediately ? "yes" : "no");
            addText(modValues, "fullChargeRequest", mod.fullChgReq ? "yes" : "no");

            auto modLimits = module["limits"].to<JsonObject>();
            addValue(modLimits, "chargeVoltage", mod.maxChgVoltV, "V", 1);
            addValue(modLimits, "dischargeVoltageLimitation", mod.minDsgVoltV, "V", 1);
            addValue(modLimits, "chargeCurrentLimitation", mod.maxChgCurrA, "A", 1);
            addValue(modLimits, "dischargeCurrentLimitation", mod.maxDsgCurrA, "A", 1);
        }

        if (mod.hasAnalog) {
            auto modCapacities = module["capacities"].to<JsonObject>();
            if (mod.totalCapacityMah > 0) {
                addValue(modCapacities, "capacity", mod.totalCapacityMah / 1000.0f, "Ah", 2);
            }
            if (mod.remainCapacityMah > 0) {
                addValue(modCapacities, "availableCapacity", mod.remainCapacityMah / 1000.0f, "Ah", 2);
            }
            if (mod.chargeCycles >= 0) {
                addValue(modCapacities, "chargeCycles", static_cast<float>(mod.chargeCycles), "", 0);
            }

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

        // Cells are sent as plain number rows plus one column description
        // (label, unit, decimals) instead of a {"v","u","d"} object per value.
        // The live view JSON is rebuilt for every websocket push and HTTP
        // request and kept in memory until sent. With per-value objects the
        // cells made up most of the document (2 modules x 16 cells: 8.8 KB
        // serialized, 20+ KB in RAM) and parallel requests drove the free
        // heap below 30 KB. This layout roughly halves the cell part and
        // scales much better with larger stacks.
        if (mod.hasCells && !mod.cells.empty()) {
            auto columns = module["cellColumns"].to<JsonArray>();
            auto addColumn = [&columns](char const* name, char const* unit, uint8_t decimals) {
                auto col = columns.add<JsonObject>();
                col["name"] = name;
                col["u"] = unit;
                col["d"] = decimals;
            };
            addColumn("voltage", "V", 3);
            addColumn("SoC", "%", 0);
            addColumn("temperature", "°C", 1);

            auto cells = module["cells"].to<JsonArray>();
            for (auto const& c : mod.cells) {
                auto row = cells.add<JsonArray>();
                row.add(c.voltageV);
                row.add(c.soc);
                row.add(c.temperatureC);
            }
        }

    }
}

// ---------------------------------------------------------------------------
// mqttPublish
// ---------------------------------------------------------------------------
void Stats::mqttPublish() const
{
    std::lock_guard<std::mutex> lock(_mutex);
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
        MqttSettings.publish("battery/CellMinTemperature", String(*oTempMin, 1));
    }
    if (oTempMax.has_value()) {
        MqttSettings.publish("battery/CellMaxTemperature", String(*oTempMax, 1));
    }

    auto publishDp = [](char const* topic, auto const& o) {
        if (o.has_value()) { MqttSettings.publish(topic, String(*o)); }
    };
    auto publishCellName = [](char const* topic, std::optional<std::string> const& o) {
        if (o) { MqttSettings.publish(topic, o->c_str()); }
    };
    publishCellName("battery/CellMinVoltageName", extremeCellName(&BatteryModule::cellMinV, &BatteryModule::cellMinNo, false));
    publishCellName("battery/CellMaxVoltageName", extremeCellName(&BatteryModule::cellMaxV, &BatteryModule::cellMaxNo, true));
    publishCellName("battery/CellMinTemperatureName", extremeCellName(&BatteryModule::tempMinC, &BatteryModule::tempMinNo, false));
    publishCellName("battery/CellMaxTemperatureName", extremeCellName(&BatteryModule::tempMaxC, &BatteryModule::tempMaxNo, true));
    publishDp("battery/modulesTotal", _dataPoints.get<Label::ModuleCount>());
    publishDp("battery/status", _dataPoints.get<Label::StatusBitmask>());
    publishDp("battery/errorStatus", _dataPoints.get<Label::ErrorBitmask>());
    auto oFullChg = _dataPoints.get<Label::FullChargeRequest>();
    if (oFullChg.has_value()) { MqttSettings.publish("battery/charging/fullChargeRequest", String(*oFullChg ? 1 : 0)); }

    // Alarms
    auto oAlarms = _dataPoints.get<Label::AlarmsBitmask>();
    if (oAlarms.has_value()) {
        uint16_t alarms = *oAlarms;
#define ALARM(name, value, key) MqttSettings.publish("battery/alarm/" key, String((alarms & static_cast<uint16_t>(AlarmBits::name)) != 0));
        PYTESRS485_ALARM_BITS(ALARM)
#undef ALARM
    }

    // Warnings
    auto oWarnings = _dataPoints.get<Label::WarningsBitmask>();
    if (oWarnings.has_value()) {
        uint16_t warnings = *oWarnings;
#define WARNING(name, value, key) MqttSettings.publish("battery/warning/" key, String((warnings & static_cast<uint16_t>(WarningBits::name)) != 0));
        PYTESRS485_WARNING_BITS(WARNING)
#undef WARNING
    }

    auto oImm = _dataPoints.get<Label::ChargeImmediately>();
    if (oImm.has_value()) {
        MqttSettings.publish("battery/charging/chargeImmediately", String(*oImm ? 1 : 0));
    }

    // same topics as the Pytes CAN provider
    auto counts = getModuleCounts();
    MqttSettings.publish("battery/modulesBlockingCharge", String(counts.blockingCharge));
    MqttSettings.publish("battery/modulesBlockingDischarge", String(counts.blockingDischarge));

    auto oBalancing = isBalancing();
    if (oBalancing) { MqttSettings.publish("battery/balancingActive", String(*oBalancing ? 1 : 0)); }

    // Per-module data
    for (size_t i = 0; i < _modules.size(); ++i) {
        auto const& mod = _modules[i];
        // keyed by serial, not by module number: the BMS assigns numbers by the
        // position in the link cable chain, so they change when modules are
        // re-wired, added or removed. Value names match the battery-level topics (Cell... for cell stats).
        if (!isUsableSerial(mod.serial)) { continue; }
        String prefix = "battery/" + mod.serial + "/";

        auto pub = [&prefix](char const* topic, String const& value) {
            MqttSettings.publish(prefix + topic, value);
        };

        // stop publishing (retained) values of an offline module, only flag it
        pub("online", String(isOnline(mod) ? 1 : 0));
        if (!isOnline(mod)) { continue; }

        pub("name", moduleName(i));
        pub("number", String(static_cast<int>(i + 1)));

        if (mod.hasBasic) {
            pub("serial",       mod.serial);
            pub("hwVersion",    mod.hwVersion);
            pub("fwVersion",    mod.swVersion);
        }

        if (mod.hasAnalog) {
            pub("voltage",           String(mod.voltageV, 3));
            pub("current",           String(mod.currentA, 3));
            pub("power",             String(mod.voltageV * mod.currentA, 1));
            pub("stateOfCharge",     String(mod.soc));
            if (mod.health > 0) { pub("stateOfHealth", String(mod.health)); }
            if (mod.totalCapacityMah > 0) {
                pub("capacity",          String(mod.totalCapacityMah  / 1000.0f, 2));
                pub("availableCapacity", String(mod.remainCapacityMah / 1000.0f, 2));
            }
            pub("ambientTemperature", String(mod.ambientTemp, 1));
            if (mod.chargeCycles >= 0) { pub("chargeCycles", String(mod.chargeCycles)); }
            auto oBalancing = isBalancing(mod);
            if (oBalancing) { pub("balancingActive", String(*oBalancing ? 1 : 0)); }
            pub("balancingState",    String(mod.balance));
            pub("status",            String(mod.status));
            pub("errorStatus",       String(mod.errorStatus));
            pub("CellMinMilliVolt",  String(static_cast<int>(mod.cellMinV * 1000.0f + 0.5f)));
            pub("CellMaxMilliVolt",  String(static_cast<int>(mod.cellMaxV * 1000.0f + 0.5f)));
            pub("CellDiffMilliVolt", String(static_cast<int>((mod.cellMaxV - mod.cellMinV) * 1000.0f + 0.5f)));
            pub("CellMinVoltageName", String(mod.cellMinNo));
            pub("CellMaxVoltageName", String(mod.cellMaxNo));
            pub("CellMinTemperature", String(mod.tempMinC, 1));
            pub("CellMaxTemperature", String(mod.tempMaxC, 1));
            pub("CellMinTemperatureName", String(mod.tempMinNo));
            pub("CellMaxTemperatureName", String(mod.tempMaxNo));
        }

        auto oModTemp = averageCellTemperature(mod);
        if (oModTemp) { pub("temperature", String(*oModTemp, 1)); }

        if (mod.hasChgDsg) {
            pub("settings/chargeVoltage",              String(mod.maxChgVoltV, 3));
            pub("settings/dischargeVoltageLimitation", String(mod.minDsgVoltV, 3));
            pub("settings/chargeCurrentLimitation",    String(mod.maxChgCurrA, 3));
            pub("settings/dischargeCurrentLimitation", String(mod.maxDsgCurrA, 3));
            pub("charging/chargeImmediately",          String(mod.chargeImmediately ? 1 : 0));
            pub("charging/fullChargeRequest",          String(mod.fullChgReq ? 1 : 0));
        }


        for (size_t j = 0; j < mod.cells.size(); ++j) {
            auto const& c = mod.cells[j];
            String cp = "cell/" + String(static_cast<int>(j + 1)) + "/";
            pub((cp + "voltage").c_str(),       String(c.voltageV, 3));
            pub((cp + "temperature").c_str(),   String(c.temperatureC, 1));
            pub((cp + "current").c_str(),       String(c.currentA, 3));
            pub((cp + "stateOfCharge").c_str(), String(c.soc));
            pub((cp + "status").c_str(),        String(c.status));
        }
    }
}

} // namespace Batteries::Pytes::Rs485
