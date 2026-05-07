// SPDX-License-Identifier: GPL-2.0-or-later
#include <Arduino.h>
#include <battery/pytes/rs485/Parsers.h>
#include <LogHelper.h>

#undef TAG
static const char* TAG = "battery";
static const char* SUBTAG = "Pytes RS485";

namespace Batteries::Pytes::Rs485::Parsers {

// ---------------------------------------------------------------------------
// Sentinel check helpers (unsupported field values per spec §1.3.5)
// ---------------------------------------------------------------------------
static bool isUint32Supported(uint32_t v) { return v != 0xFFFFFFFFu; }
static bool isSint32Supported(int32_t v)  { return v != 0x7FFFFFFF; }
static bool isUint16Supported(uint16_t v) { return v != 0xFFFFu; }

// ---------------------------------------------------------------------------
// parsePackBasic (CID2 = 0x60)
// ---------------------------------------------------------------------------
// manufacturer(20) hw_version(32) sw_version(32) n_modules(U16)
// [n_modules × serial(16)] [pack_sn_len(U8) pack_sn(var)]
DataPointContainer parsePackBasic(SerialResponse const& response)
{
    DataPointContainer dp;
    auto pos = response.begin();

    String manufacturer = response.getString(pos, 20);
    response.getString(pos, 32); // skip hw_version
    response.getString(pos, 32); // skip sw_version
    uint16_t nBatt      = response.getU16(pos);

    DTU_LOGI("Manufacturer: %s Batteries: %u", manufacturer.c_str(), nBatt);

    if (!manufacturer.isEmpty()) {
        dp.add<DataPointLabel::PackManufacturer>(std::string(manufacturer.c_str()));
    }

    dp.add<DataPointLabel::ModuleCount>(static_cast<uint8_t>(nBatt > 0 ? nBatt : 1));

    return dp;
}

// ---------------------------------------------------------------------------
// parsePackAnalog (CID2 = 0x61)
// ---------------------------------------------------------------------------
// n_modules(U16) status(U32) error_status(U32)
// total_mah(U32) remain_mah(U32) soc(U16) health(U16)
// voltage_mv(S32) current_ma(S32)
// cell_max_mv(S32) cell_max_no(U8) cell_min_mv(S32) cell_min_no(U8)
// temp_max_mc(S32) temp_max_no(U8) temp_min_mc(S32) temp_min_no(U8)
// [acc_dsg(U32) acc_chg(U32)]  — optional
DataPointContainer parsePackAnalog(SerialResponse const& response)
{
    DataPointContainer dp;
    auto pos = response.begin();

    response.getU16(pos); // skip n_modules
    uint32_t status     = response.getU32(pos);
    uint32_t errStatus  = response.getU32(pos);
    uint32_t totalMah   = response.getU32(pos);
    uint32_t remainMah  = response.getU32(pos);
    uint16_t soc        = response.getU16(pos);
    uint16_t health     = response.getU16(pos);
    int32_t  voltageMv  = response.getS32(pos);
    int32_t  currentMa  = response.getS32(pos);
    int32_t  cellMaxMv  = response.getS32(pos);
    uint8_t  cellMaxNo  = response.getU8(pos); // TODO(andreasboehm): skip
    int32_t  cellMinMv  = response.getS32(pos);
    uint8_t  cellMinNo  = response.getU8(pos);// TODO(andreasboehm): skip
    int32_t  tempMaxMc  = response.getS32(pos);
    uint8_t  tempMaxNo  = response.getU8(pos);// TODO(andreasboehm): skip
    int32_t  tempMinMc  = response.getS32(pos);
    uint8_t  tempMinNo  = response.getU8(pos);// TODO(andreasboehm): skip
    uint32_t accDsg     = response.getU32(pos);
    uint32_t accChg     = response.getU32(pos);

    if (isUint16Supported(static_cast<uint16_t>(health))) {
        dp.add<DataPointLabel::BatterySoHPercent>(static_cast<uint16_t>(health));
    }
    if (isSint32Supported(voltageMv)) {
        dp.add<DataPointLabel::BatteryVoltageMilliVolt>(static_cast<uint32_t>(voltageMv));
    }
    if (isSint32Supported(currentMa)) {
        dp.add<DataPointLabel::BatteryCurrentMilliAmps>(currentMa);
    }
    if (isUint32Supported(totalMah)) {
        dp.add<DataPointLabel::TotalCapacityMilliAmpHours>(totalMah);
    }
    if (isUint32Supported(remainMah)) {
        dp.add<DataPointLabel::RemainingCapacityMilliAmpHours>(remainMah);
    }

    // use more precise calculated SoC when possible
    if (isUint32Supported(totalMah) && isUint32Supported(remainMah)) {
        float calculatedSoc = 100.0 * remainMah / totalMah;
        dp.add<DataPointLabel::BatterySoCPercent>(calculatedSoc);

    } else {
        dp.add<DataPointLabel::BatterySoCPercent>(static_cast<float>(soc));
    }

    if (isSint32Supported(cellMaxMv)) {
        dp.add<DataPointLabel::CellMaxMilliVolt>(static_cast<uint16_t>(cellMaxMv));
    }
    if (isSint32Supported(cellMinMv)) {
        dp.add<DataPointLabel::CellMinMilliVolt>(static_cast<uint16_t>(cellMinMv));
    }

    if (isSint32Supported(tempMaxMc)) {
        dp.add<DataPointLabel::CellMaxTemperatureCelsius>(static_cast<int16_t>(tempMaxMc / 1000));
    }
    if (isSint32Supported(tempMinMc)) {
        dp.add<DataPointLabel::CellMinTemperatureCelsius>(static_cast<int16_t>(tempMinMc / 1000));
    }

    if (isUint32Supported(accDsg)) {
        dp.add<DataPointLabel::AccumulatedDischargeDeciKWh>(accDsg);
    }
    if (isUint32Supported(accChg)) {
        dp.add<DataPointLabel::AccumulatedChargeDeciKWh>(accChg);
    }

    if (errStatus != 0) { DTU_LOGW("Pack error status: 0x%08X", errStatus); }

    auto statusBit = [&](uint32_t b) { return (status & (1u << b)) != 0; };

    bool stateCharging    = statusBit(26);
    bool stateDischarging = statusBit(27);

    // TODO(andreasboehm): check if this is handled correctly
    uint16_t alarms = 0;
    if (statusBit(0))                           { alarms |= static_cast<uint16_t>(AlarmBits::OverVoltage); }
    if (statusBit(4))                           { alarms |= static_cast<uint16_t>(AlarmBits::UnderVoltage); }
    if (stateDischarging && statusBit(8))       { alarms |= static_cast<uint16_t>(AlarmBits::OverTemperature); }
    if (stateDischarging && statusBit(12))      { alarms |= static_cast<uint16_t>(AlarmBits::UnderTemperature); }
    if (stateCharging && statusBit(8))          { alarms |= static_cast<uint16_t>(AlarmBits::OverTemperatureCharge); }
    if (stateCharging && statusBit(12))         { alarms |= static_cast<uint16_t>(AlarmBits::UnderTemperatureCharge); }
    if (statusBit(19) || statusBit(17))         { alarms |= static_cast<uint16_t>(AlarmBits::OverCurrentDischarge); }
    if (statusBit(20) || statusBit(18))         { alarms |= static_cast<uint16_t>(AlarmBits::OverCurrentCharge); }
    if (statusBit(28))                          { alarms |= static_cast<uint16_t>(AlarmBits::InternalFailure); }
    // CellImbalance: always false per protocol

    // TODO(andreasboehm): check if this is handled correctly
    uint16_t warnings = 0;
    if (statusBit(1))                           { warnings |= static_cast<uint16_t>(WarningBits::HighVoltage); }
    if (statusBit(3))                           { warnings |= static_cast<uint16_t>(WarningBits::LowVoltage); }
    if (stateDischarging && statusBit(9))       { warnings |= static_cast<uint16_t>(WarningBits::HighTemperature); }
    if (stateDischarging && statusBit(11))      { warnings |= static_cast<uint16_t>(WarningBits::LowTemperature); }
    if (stateCharging && statusBit(9))          { warnings |= static_cast<uint16_t>(WarningBits::HighTemperatureCharge); }
    if (stateCharging && statusBit(11))         { warnings |= static_cast<uint16_t>(WarningBits::LowTemperatureCharge); }
    if (statusBit(21))                          { warnings |= static_cast<uint16_t>(WarningBits::HighCurrentDischarge); }
    if (statusBit(22))                          { warnings |= static_cast<uint16_t>(WarningBits::HighCurrentCharge); }
    // InternalFailure and CellImbalance warnings: always false per protocol

    dp.add<DataPointLabel::AlarmsBitmask>(alarms);
    dp.add<DataPointLabel::WarningsBitmask>(warnings);

    DTU_LOGD("V=%dmV I=%dmA SoC=%u%% SoH=%u%% cellMax=%dmV cellMin=%dmV status=0x%08X",
             voltageMv, currentMa, soc, health, cellMaxMv, cellMinMv, status);

    return dp;
}

// ---------------------------------------------------------------------------
// parsePackChgDsg (CID2 = 0x62)
// ---------------------------------------------------------------------------
// max_chg_voltage_mv(U32) min_dsg_voltage_mv(U32)
// max_chg_current_ma(U32) max_dsg_current_ma(U32)
// emerg_chg_flag1(U8) emerg_chg_flag2(U8) full_chg_request(U8)
DataPointContainer parsePackChgDsg(SerialResponse const& response)
{
    DataPointContainer dp;
    auto pos = response.begin();

    uint32_t maxChgVoltMv = response.getU32(pos);
    uint32_t minDsgVoltMv = response.getU32(pos);
    uint32_t maxChgCurrMa = response.getU32(pos);
    uint32_t maxDsgCurrMa = response.getU32(pos);
    uint8_t  emergFlag1   = response.getU8(pos);
    uint8_t  emergFlag2   = response.getU8(pos);
    uint8_t  fullChgReq   = response.getU8(pos);

    if (isUint32Supported(maxChgVoltMv)) {
        dp.add<DataPointLabel::ChargeVoltageLimitMilliVolt>(maxChgVoltMv);
    }
    if (isUint32Supported(minDsgVoltMv)) {
        dp.add<DataPointLabel::DischargeVoltageLimitMilliVolt>(minDsgVoltMv);
    }
    if (isUint32Supported(maxChgCurrMa)) {
        dp.add<DataPointLabel::ChargeCurrentLimitMilliAmps>(maxChgCurrMa);
    }
    if (isUint32Supported(maxDsgCurrMa)) {
        dp.add<DataPointLabel::DischargeCurrentLimitMilliAmps>(maxDsgCurrMa);
    }

    // TODO(andreasboehm): Check if we should handle those flags
    if (emergFlag1 != 0 || emergFlag2 != 0) {
        DTU_LOGW("Emergency charge flags: flag1=%u flag2=%u", emergFlag1, emergFlag2);
    }

    dp.add<DataPointLabel::ChargeImmediately>(fullChgReq != 0);

    DTU_LOGD("maxChgV=%umV minDsgV=%umV maxChgI=%umA maxDsgI=%umA fullChgReq=%u",
             maxChgVoltMv, minDsgVoltMv, maxChgCurrMa, maxDsgCurrMa, fullChgReq);

    return dp;
}

// ---------------------------------------------------------------------------
// parseModuleBasic (CID2 = 0x80, CID2 byte already stripped by SerialResponse)
// ---------------------------------------------------------------------------
// module_no(U8) n_cells(U8) manufacturer(20) hw_version(32) sw_version(32) serial(16)
ModuleBasicResult parseModuleBasic(SerialResponse const& response)
{
    ModuleBasicResult r;
    auto pos = response.begin();

    r.moduleNo      = response.getU8(pos);
    response.getU8(pos); // skip cell count
    response.getString(pos, 20); // skip manufacturer
    r.hwVersion     = response.getString(pos, 32);
    r.swVersion     = response.getString(pos, 32);
    r.serial        = response.getString(pos, 16);

    DTU_LOGI("MOD%u: hw=%s sw=%s sn=%s",
             r.moduleNo, r.hwVersion.c_str(), r.swVersion.c_str(),
             r.serial.c_str());

    return r;
}

// ---------------------------------------------------------------------------
// parseModuleAnalog (CID2 = 0x81, CID2 byte already stripped)
// ---------------------------------------------------------------------------
// module_no(U8) n_cells(U8) status(U32) error_status(U32)
// total_mah(U32) remain_mah(U32) soc(U16) health(U16)
// voltage_mv(S32) current_ma(S32) ambient_mc(S32) equil_state(U16) cycles(U32)
ModuleAnalogResult parseModuleAnalog(SerialResponse const& response)
{
    ModuleAnalogResult r;
    auto pos = response.begin();

    r.moduleNo = response.getU8(pos);
    response.getU8(pos); // skip n_cells (already parsed as paort of ModuleBasic)
    response.getU32(pos); // skip status (TODO(andreasboehm): check if we want to parse it)
    response.getU32(pos); // skip error_status (TODO(andreasboehm): check if we want to parse it)
    uint32_t totalMah   = response.getU32(pos);
    uint32_t remainMah  = response.getU32(pos);
    uint16_t soc        = response.getU16(pos);
    uint16_t health     = response.getU16(pos);
    int32_t  voltMv     = response.getS32(pos);
    int32_t  currMa     = response.getS32(pos);
    int32_t  ambientMc  = response.getS32(pos);
    uint16_t equil      = response.getU16(pos);
    uint32_t cycles     = response.getU32(pos);
    int32_t  cellMaxMv  = response.getS32(pos);
    uint8_t  cellMaxNo  = response.getU8(pos);
    int32_t  cellMinMv  = response.getS32(pos);
    uint8_t  cellMinNo  = response.getU8(pos);
    int32_t  tempMaxMc  = response.getS32(pos);
    uint8_t  tempMaxNo  = response.getU8(pos);
    int32_t  tempMinMc  = response.getS32(pos);
    uint8_t  tempMinNo  = response.getU8(pos);

    r.voltageV         = isSint32Supported(voltMv)  ? voltMv  / 1000.0f : 0.0f;
    r.currentA         = isSint32Supported(currMa)  ? currMa  / 1000.0f : 0.0f;

    // use more precise calculated SoC when possible
    if (isUint32Supported(totalMah) && isUint32Supported(remainMah)) {
        r.soc           = 100.0 * remainMah / totalMah;

    } else {
        r.soc           = static_cast<float>(soc);
    }

    r.health            = isUint16Supported(health)  ? health  : 0;
    r.totalCapacityMah  = isUint32Supported(totalMah)  ? totalMah  : 0;
    r.remainCapacityMah = isUint32Supported(remainMah) ? remainMah : 0;
    r.ambientTemp       = isSint32Supported(ambientMc) ? ambientMc / 1000.0f : 0.0f;
    r.chargeCycles      = isUint32Supported(cycles) ? static_cast<int>(cycles) : -1;
    r.balance           = equil;
    r.cellMaxV          = isSint32Supported(cellMaxMv) ? cellMaxMv / 1000.0f : 0.0f;
    r.cellMaxNo         = cellMaxNo;
    r.cellMinV          = isSint32Supported(cellMinMv) ? cellMinMv / 1000.0f : 0.0f;
    r.cellMinNo         = cellMinNo;
    r.tempMaxC          = isSint32Supported(tempMaxMc) ? tempMaxMc / 1000.0f : 0.0f;
    r.tempMaxNo         = tempMaxNo;
    r.tempMinC          = isSint32Supported(tempMinMc) ? tempMinMc / 1000.0f : 0.0f;
    r.tempMinNo         = tempMinNo;

    DTU_LOGD("MOD%u: %.3fV %+.3fA SoC=%u%% SoH=%u%% cap=%u/%umAh ambient=%.1f°C cycles=%u "
             "cellMax=%.3fV(C%u) cellMin=%.3fV(C%u) tempMax=%.1f°C(C%u) tempMin=%.1f°C(C%u)",
             r.moduleNo, r.voltageV, r.currentA, r.soc, r.health,
             r.remainCapacityMah, r.totalCapacityMah, r.ambientTemp, r.chargeCycles,
             r.cellMaxV, r.cellMaxNo, r.cellMinV, r.cellMinNo,
             r.tempMaxC, r.tempMaxNo, r.tempMinC, r.tempMinNo);

    return r;
}

// ---------------------------------------------------------------------------
// parseModuleChgDsg (CID2 = 0x83, CID2 byte already stripped)
// ---------------------------------------------------------------------------
// module_no(U8) max_chg_voltage_mv(U32) min_dsg_voltage_mv(U32)
// max_chg_current_ma(U32) max_dsg_current_ma(U32)
// emerg_chg_flag1(U8) emerg_chg_flag2(U8) full_chg_request(U8)
ModuleChgDsgResult parseModuleChgDsg(SerialResponse const& response)
{
    ModuleChgDsgResult r;
    auto pos = response.begin();

    r.moduleNo            = response.getU8(pos);
    uint32_t maxChgVoltMv = response.getU32(pos);
    uint32_t minDsgVoltMv = response.getU32(pos);
    uint32_t maxChgCurrMa = response.getU32(pos);
    uint32_t maxDsgCurrMa = response.getU32(pos);
    uint32_t emergFlag1   = response.getU8(pos);
    uint32_t emergFlag2   = response.getU8(pos);
    uint32_t fullChgReq   = response.getU8(pos);

    r.maxChgVoltV = isUint32Supported(maxChgVoltMv) ? maxChgVoltMv / 1000.0f : 0.0f;
    r.minDsgVoltV = isUint32Supported(minDsgVoltMv) ? minDsgVoltMv / 1000.0f : 0.0f;
    r.maxChgCurrA = isUint32Supported(maxChgCurrMa) ? maxChgCurrMa / 1000.0f : 0.0f;
    r.maxDsgCurrA = isUint32Supported(maxDsgCurrMa) ? maxDsgCurrMa / 1000.0f : 0.0f;
    r.emergFlags  = static_cast<uint8_t>(emergFlag1 | (emergFlag2 << 4));
    r.fullChgReq  = (fullChgReq != 0);

    if (emergFlag1 != 0 || emergFlag2 != 0) {
        DTU_LOGW("MOD%u emergency charge flags: f1=%u f2=%u", r.moduleNo, emergFlag1, emergFlag2);
    }

    DTU_LOGD("MOD%u: maxChgV=%.1fV minDsgV=%.1fV maxChgI=%.1fA maxDsgI=%.1fA fullChgReq=%u",
             r.moduleNo, r.maxChgVoltV, r.minDsgVoltV, r.maxChgCurrA, r.maxDsgCurrA, fullChgReq);

    return r;
}

// ---------------------------------------------------------------------------
// parseModuleCells (CID2 = 0x92, CID2 byte already stripped)
// ---------------------------------------------------------------------------
// module_no(U8) n_cells(U8)
// Per cell: status(U32) soc(U16) voltage_mv(S32) current_ma(U32) temp_mc(U32)
ModuleCellsResult parseModuleCells(SerialResponse const& response)
{
    ModuleCellsResult r;
    auto pos = response.begin();

    r.moduleNo      = response.getU8(pos);
    uint8_t nCells  = response.getU8(pos);

    r.cells.reserve(nCells);

    for (uint8_t i = 0; i < nCells; ++i) {
        uint32_t cellStatus = response.getU32(pos);
        uint32_t cellSoc    = response.getU16(pos); // TODO(andreasboehm): skip
        int32_t  voltageMv  = response.getS32(pos);
        int32_t  currentMa  = response.getS32(pos); // TODO(andreasboehm): skip
        uint32_t tempMc     = response.getU32(pos);

        CellData cd;
        cd.voltageV     = isSint32Supported(voltageMv) ? voltageMv / 1000.0f : 0.0f;
        cd.temperatureC = isUint32Supported(tempMc)    ? tempMc    / 1000.0f : 0.0f;

        r.cells.push_back(cd);

        DTU_LOGD("  MOD%u Cell %2u: %.3f V %.1f°C status=0x%08X",
                 r.moduleNo, i + 1, cd.voltageV, cd.temperatureC, cellStatus);
    }

    return r;
}

// ---------------------------------------------------------------------------
// parseModuleProtect (CID2 = 0x82, CID2 byte already stripped)
// ---------------------------------------------------------------------------
// module_no(U8) then protection parameter fields
ModuleProtectResult parseModuleProtect(SerialResponse const& response)
{
    ModuleProtectResult r;
    auto pos = response.begin();

    r.moduleNo = response.getU8(pos);

    uint32_t overVoltProt   = response.getU32(pos); // overvoltage protection
    response.getU32(pos);                            // overvoltage recovery
    uint32_t highVoltAlarm  = response.getU32(pos); // high voltage alarm
    response.getU32(pos);                            // high voltage recovery
    uint32_t lowVoltAlarm   = response.getU32(pos); // low voltage alarm
    response.getU32(pos);                            // low voltage recovery
    uint32_t underVoltProt  = response.getU32(pos); // undervoltage protection
    response.getU32(pos);                            // undervoltage recovery
    response.getU32(pos);                            // sleep voltage

    int32_t chgOverTempProt  = response.getS32(pos); // charge over-temp protection
    response.getS32(pos);                             // recovery
    int32_t chgHighTempAlarm = response.getS32(pos); // charge high-temp alarm
    response.getS32(pos);                             // recovery
    int32_t chgLowTempAlarm  = response.getS32(pos); // charge low-temp alarm
    response.getS32(pos);                             // recovery
    int32_t chgUnderTempProt = response.getS32(pos); // charge under-temp protection
    response.getS32(pos);                             // recovery

    int32_t dsgOverTempProt  = response.getS32(pos); // discharge over-temp protection
    response.getS32(pos);                             // recovery
    int32_t dsgHighTempAlarm = response.getS32(pos); // discharge high-temp alarm
    response.getS32(pos);                             // recovery
    int32_t dsgLowTempAlarm  = response.getS32(pos); // discharge low-temp alarm
    response.getS32(pos);                             // recovery
    int32_t dsgUnderTempProt = response.getS32(pos); // discharge under-temp protection
    response.getS32(pos);                             // recovery

    int32_t chgOvercurrProt  = response.getS32(pos); // charge overcurrent protection
    response.getS32(pos);                             // alarm
    response.getS32(pos);                             // recovery
    int32_t dsgOvercurrProt  = response.getS32(pos); // discharge overcurrent protection
    response.getS32(pos);                             // alarm
    response.getS32(pos);                             // recovery
    response.getS32(pos);                             // delay
    response.getS32(pos);                             // recovery time

    response.getS32(pos);                             // short circuit protection
    response.getU32(pos);                             // delay
    response.getU32(pos);                             // recovery

    response.getU32(pos);                             // charge current limit
    uint32_t balanceStartMv  = response.getU32(pos); // equalization start voltage
    uint32_t balanceDiffMv   = response.getU32(pos); // balancing voltage difference

    DTU_LOGD("MOD%u protect: OVP=%umV UVP=%umV chgOTP=%.1f°C dsgOTP=%.1f°C",
             r.moduleNo, overVoltProt, underVoltProt,
             chgOverTempProt / 1000.0f, dsgOverTempProt / 1000.0f);

    auto& p = r.protect;
    if (isUint32Supported(overVoltProt))    { p.overvoltageProtectionMv  = overVoltProt; }
    if (isUint32Supported(underVoltProt))   { p.undervoltageProtectionMv = underVoltProt; }
    if (isUint32Supported(highVoltAlarm))   { p.highVoltageAlarmMv       = highVoltAlarm; }
    if (isUint32Supported(lowVoltAlarm))    { p.lowVoltageAlarmMv        = lowVoltAlarm; }
    if (isSint32Supported(chgOverTempProt))  { p.chargeOverTempProtMc   = chgOverTempProt; }
    if (isSint32Supported(chgUnderTempProt)) { p.chargeUnderTempProtMc  = chgUnderTempProt; }
    if (isSint32Supported(chgHighTempAlarm)) { p.chargeHighTempAlarmMc  = chgHighTempAlarm; }
    if (isSint32Supported(chgLowTempAlarm))  { p.chargeLowTempAlarmMc   = chgLowTempAlarm; }
    if (isSint32Supported(dsgOverTempProt))  { p.dischargeOverTempProtMc  = dsgOverTempProt; }
    if (isSint32Supported(dsgUnderTempProt)) { p.dischargeUnderTempProtMc = dsgUnderTempProt; }
    if (isSint32Supported(dsgHighTempAlarm)) { p.dischargeHighTempAlarmMc = dsgHighTempAlarm; }
    if (isSint32Supported(dsgLowTempAlarm))  { p.dischargeLowTempAlarmMc  = dsgLowTempAlarm; }
    if (isSint32Supported(chgOvercurrProt))  { p.chargeOvercurrentMa    = chgOvercurrProt; }
    if (isSint32Supported(dsgOvercurrProt))  { p.dischargeOvercurrentMa = dsgOvercurrProt; }
    if (isUint32Supported(balanceStartMv))   { p.balanceStartVoltageMv  = balanceStartMv; }
    if (isUint32Supported(balanceDiffMv))    { p.balanceDiffMv          = balanceDiffMv; }

    return r;
}

} // namespace Batteries::Pytes::Rs485::Parsers
