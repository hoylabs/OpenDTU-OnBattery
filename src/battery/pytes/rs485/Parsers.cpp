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

// cell numbers reported by the BMS are 0-based (see spec 4.3 and verified on
// a real battery), we present them 1-based like the per-cell lists
// emergency charge flags map to "charge immediately" (like Pylontech's
// "charge immediately" bits), 0xFF means unsupported
static bool emergencyCharge(uint8_t flag1, uint8_t flag2)
{
    auto set = [](uint8_t f) { return f != 0 && f != 0xFF; };
    return set(flag1) || set(flag2);
}

static uint8_t cellNo(uint8_t v) { return v == 0xFF ? v : v + 1; }

// ---------------------------------------------------------------------------
// parseClusterBasic (CID2 = 0x60)
// ---------------------------------------------------------------------------
// manufacturer(20) hw_version(32) sw_version(32) n_modules(U16)
// [n_modules × serial(16)] [cluster_sn_len(U8) cluster_sn(var)]
DataPointContainer parseClusterBasic(SerialResponse const& response)
{
    DataPointContainer dp;
    auto pos = response.begin();

    String manufacturer = response.getString(pos, 20);
    response.getString(pos, 32); // skip hw_version, differs per module, see 0x80
    response.getString(pos, 32); // skip sw_version, differs per module, see 0x80
    uint16_t nBatt      = response.getU16(pos);
    // ponytail: per-battery serials are read via 0x80, cluster S/N layout differs between LV/HV and is skipped

    DTU_LOGI("Manufacturer: %s Batteries: %u", manufacturer.c_str(), nBatt);

    if (!manufacturer.isEmpty()) {
        dp.add<DataPointLabel::Manufacturer>(std::string(manufacturer.c_str()));
    }

    dp.add<DataPointLabel::ModuleCount>(static_cast<uint8_t>(nBatt > 0 ? nBatt : 1));

    return dp;
}

// ---------------------------------------------------------------------------
// parseClusterAnalog (CID2 = 0x61)
// ---------------------------------------------------------------------------
// n_modules(U16) status(U32) error_status(U32)
// total_mah(U32) remain_mah(U32) soc(U16) health(U16)
// voltage_mv(S32) current_ma(S32)
// cell_max_mv(S32) cell_max_no(U8) cell_min_mv(S32) cell_min_no(U8)
// temp_max_mc(S32) temp_max_no(U8) temp_min_mc(S32) temp_min_no(U8)
// [acc_dsg(U32) acc_chg(U32)]  — optional
DataPointContainer parseClusterAnalog(SerialResponse const& response)
{
    DataPointContainer dp;
    auto pos = response.begin();

    uint16_t nBatt      = response.getU16(pos);
    uint32_t status     = response.getU32(pos);
    uint32_t errStatus  = response.getU32(pos);
    uint32_t totalMah   = response.getU32(pos);
    uint32_t remainMah  = response.getU32(pos);
    uint16_t soc        = response.getU16(pos);
    uint16_t health     = response.getU16(pos);
    int32_t  voltageMv  = response.getS32(pos);
    int32_t  currentMa  = response.getS32(pos);
    int32_t  cellMaxMv  = response.getS32(pos);
    // cell numbers are skipped: they lack the module number, so they are
    // ambiguous with more than one module. Stats derives them from 0x81.
    response.getU8(pos);
    int32_t  cellMinMv  = response.getS32(pos);
    response.getU8(pos);
    int32_t  tempMaxMc  = response.getS32(pos);
    response.getU8(pos);
    int32_t  tempMinMc  = response.getS32(pos);
    response.getU8(pos);
    uint32_t accDsg     = response.getU32(pos);
    uint32_t accChg     = response.getU32(pos);

    // reported every cycle, lets us notice added/removed modules without a reboot
    if (nBatt > 0 && isUint16Supported(nBatt)) {
        dp.add<DataPointLabel::ModuleCount>(static_cast<uint8_t>(nBatt));
    }

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
    if (isUint32Supported(totalMah) && totalMah > 0 && isUint32Supported(remainMah)) {
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
        dp.add<DataPointLabel::CellMaxTemperatureCelsius>(tempMaxMc / 1000.0f);
    }
    if (isSint32Supported(tempMinMc)) {
        dp.add<DataPointLabel::CellMinTemperatureCelsius>(tempMinMc / 1000.0f);
    }

    dp.add<DataPointLabel::StatusBitmask>(status);
    dp.add<DataPointLabel::ErrorBitmask>(errStatus);

    if (isUint32Supported(accDsg)) {
        dp.add<DataPointLabel::AccumulatedDischargeDeciKWh>(accDsg);
    }
    if (isUint32Supported(accChg)) {
        dp.add<DataPointLabel::AccumulatedChargeDeciKWh>(accChg);
    }

    if (errStatus != 0) { DTU_LOGW("Battery error status: 0x%08X", errStatus); }

    auto statusBit = [&](uint32_t b) { return (status & (1u << b)) != 0; };

    // spec 1.3.7 has no charge/discharge specific temperature flags. Attribute
    // them to the charge variant while charging (CHG, bit 26), else to the
    // generic one so they are never dropped while idle.
    bool charging = statusBit(26);

    uint16_t alarms = 0;
    auto alarm = [&](bool set, AlarmBits bit) { if (set) { alarms |= static_cast<uint16_t>(bit); } };
    alarm(statusBit(0),                   AlarmBits::OverVoltage);           // OV
    alarm(statusBit(4),                   AlarmBits::UnderVoltage);          // UV
    alarm(!charging && statusBit(8),      AlarmBits::OverTemperature);       // OT
    alarm(!charging && statusBit(12),     AlarmBits::UnderTemperature);      // UT
    alarm(charging && statusBit(8),       AlarmBits::OverTemperatureCharge);
    alarm(charging && statusBit(12),      AlarmBits::UnderTemperatureCharge);
    alarm(statusBit(16) || statusBit(17) || statusBit(19), AlarmBits::OverCurrentDischarge); // SC, DOC2, DOC
    alarm(statusBit(18) || statusBit(20), AlarmBits::OverCurrentCharge);     // COC2, COC
    alarm(statusBit(28) || errStatus != 0, AlarmBits::InternalFailure);      // FAULT, system error

    uint16_t warnings = 0;
    auto warning = [&](bool set, WarningBits bit) { if (set) { warnings |= static_cast<uint16_t>(bit); } };
    warning(statusBit(1),                 WarningBits::HighVoltage);         // HV
    warning(statusBit(3),                 WarningBits::LowVoltage);          // LV
    warning(!charging && statusBit(9),    WarningBits::HighTemperature);     // HT
    warning(!charging && statusBit(11),   WarningBits::LowTemperature);      // LT
    warning(charging && statusBit(9),     WarningBits::HighTemperatureCharge);
    warning(charging && statusBit(11),    WarningBits::LowTemperatureCharge);
    warning(statusBit(21),                WarningBits::HighCurrentDischarge); // DOCA
    warning(statusBit(22),                WarningBits::HighCurrentCharge);    // COCA

    dp.add<DataPointLabel::AlarmsBitmask>(alarms);
    dp.add<DataPointLabel::WarningsBitmask>(warnings);

    DTU_LOGD("V=%dmV I=%dmA SoC=%u%% SoH=%u%% cellMax=%dmV cellMin=%dmV status=0x%08X",
             voltageMv, currentMa, soc, health, cellMaxMv, cellMinMv, status);

    return dp;
}

// ---------------------------------------------------------------------------
// parseClusterChgDsg (CID2 = 0x62)
// ---------------------------------------------------------------------------
// max_chg_voltage_mv(U32) min_dsg_voltage_mv(U32)
// max_chg_current_ma(U32) max_dsg_current_ma(U32)
// emerg_chg_flag1(U8) emerg_chg_flag2(U8) full_chg_request(U8)
DataPointContainer parseClusterChgDsg(SerialResponse const& response)
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


    // full charge request is a maintenance request (SoC calibration), not an
    // emergency, so it must not trigger ChargeImmediately
    dp.add<DataPointLabel::ChargeImmediately>(emergencyCharge(emergFlag1, emergFlag2));
    if (fullChgReq != 0xFF) { dp.add<DataPointLabel::FullChargeRequest>(fullChgReq != 0); }

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
    r.nCells        = response.getU8(pos);
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
    response.getU8(pos); // skip n_cells (reported by 0x92 as well)
    r.status      = response.getU32(pos);
    r.errorStatus = response.getU32(pos);
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
    if (isUint32Supported(totalMah) && totalMah > 0 && isUint32Supported(remainMah)) {
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
    r.cellMaxNo         = cellNo(cellMaxNo);
    r.cellMinV          = isSint32Supported(cellMinMv) ? cellMinMv / 1000.0f : 0.0f;
    r.cellMinNo         = cellNo(cellMinNo);
    r.tempMaxC          = isSint32Supported(tempMaxMc) ? tempMaxMc / 1000.0f : 0.0f;
    r.tempMaxNo         = cellNo(tempMaxNo);
    r.tempMinC          = isSint32Supported(tempMinMc) ? tempMinMc / 1000.0f : 0.0f;
    r.tempMinNo         = cellNo(tempMinNo);

    DTU_LOGD("MOD%u: %.3fV %+.3fA SoC=%.2f%% SoH=%u%% cap=%u/%umAh ambient=%.1f°C cycles=%d "
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
    uint8_t emergFlag1    = response.getU8(pos);
    uint8_t emergFlag2    = response.getU8(pos);
    uint8_t fullChgReq    = response.getU8(pos);

    r.maxChgVoltV = isUint32Supported(maxChgVoltMv) ? maxChgVoltMv / 1000.0f : 0.0f;
    r.minDsgVoltV = isUint32Supported(minDsgVoltMv) ? minDsgVoltMv / 1000.0f : 0.0f;
    r.maxChgCurrA = isUint32Supported(maxChgCurrMa) ? maxChgCurrMa / 1000.0f : 0.0f;
    r.maxDsgCurrA = isUint32Supported(maxDsgCurrMa) ? maxDsgCurrMa / 1000.0f : 0.0f;
    r.chargeImmediately = emergencyCharge(emergFlag1, emergFlag2);
    r.fullChgReq  = (fullChgReq != 0 && fullChgReq != 0xFF);

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
        // spec lists current and temperature as Uint32, but both are signed
        // in every other command (and in practice: discharge, sub-zero temps)
        uint32_t cellStatus = response.getU32(pos);
        uint16_t cellSoc    = response.getU16(pos);
        int32_t  voltageMv  = response.getS32(pos);
        int32_t  currentMa  = response.getS32(pos);
        int32_t  tempMc     = response.getS32(pos);

        CellData cd;
        cd.voltageV     = isSint32Supported(voltageMv) ? voltageMv / 1000.0f : 0.0f;
        cd.temperatureC = isSint32Supported(tempMc)    ? tempMc    / 1000.0f : 0.0f;
        cd.currentA     = isSint32Supported(currentMa) ? currentMa / 1000.0f : 0.0f;
        cd.soc          = isUint16Supported(cellSoc)   ? cellSoc : 0;
        cd.status       = cellStatus;

        r.cells.push_back(cd);

        DTU_LOGD("  MOD%u Cell %2u: %.3f V %.1f°C status=0x%08X",
                 r.moduleNo, i + 1, cd.voltageV, cd.temperatureC, cellStatus);
    }

    return r;
}


} // namespace Batteries::Pytes::Rs485::Parsers
