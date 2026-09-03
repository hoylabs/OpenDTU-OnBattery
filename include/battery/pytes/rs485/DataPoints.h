// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <Arduino.h>
#include <map>
#include <frozen/map.h>
#include <frozen/string.h>
#include <DataPoints.h>

namespace Batteries::Pytes::Rs485 {

// ---------------------------------------------------------------------------
// Per-cell data (used in BatteryModule)
// ---------------------------------------------------------------------------
struct CellData {
    float voltageV;     // V
    float temperatureC; // °C
};

// ---------------------------------------------------------------------------
// Per-module protection thresholds (used in BatteryModule)
// ---------------------------------------------------------------------------
struct ProtectParams {
    uint32_t overvoltageProtectionMv  = 0;
    uint32_t undervoltageProtectionMv = 0;
    uint32_t highVoltageAlarmMv       = 0;
    uint32_t lowVoltageAlarmMv        = 0;
    int32_t  chargeOverTempProtMc     = 0;
    int32_t  chargeUnderTempProtMc    = 0;
    int32_t  chargeHighTempAlarmMc    = 0;
    int32_t  chargeLowTempAlarmMc     = 0;
    int32_t  dischargeOverTempProtMc  = 0;
    int32_t  dischargeUnderTempProtMc = 0;
    int32_t  dischargeHighTempAlarmMc = 0;
    int32_t  dischargeLowTempAlarmMc  = 0;
    int32_t  chargeOvercurrentMa      = 0;
    int32_t  dischargeOvercurrentMa   = 0;
    uint32_t balanceStartVoltageMv    = 0;
    uint32_t balanceDiffMv            = 0;
};

// ---------------------------------------------------------------------------
// Per-module aggregate info (stored in Stats::_modules)
// ---------------------------------------------------------------------------
struct BatteryModule {
    bool hasBasic   = false;
    bool hasAnalog  = false;
    bool hasCells   = false;
    bool hasChgDsg  = false;
    bool hasProtect = false;
    String serial;
    String hwVersion;
    String swVersion;
    uint8_t nCells         = 0;
    float voltageV         = 0;
    float currentA         = 0;
    float soc              = 0;
    uint16_t health        = 0;
    uint32_t totalCapacityMah  = 0;
    uint32_t remainCapacityMah = 0;
    float ambientTemp      = 0;
    int chargeCycles       = -1;
    uint16_t balance       = 0;
    std::vector<CellData> cells;
    float cellMaxV      = 0;
    uint8_t cellMaxNo   = 0;
    float cellMinV      = 0;
    uint8_t cellMinNo   = 0;
    float tempMaxC      = 0;
    uint8_t tempMaxNo   = 0;
    float tempMinC      = 0;
    uint8_t tempMinNo   = 0;
    bool hasTempMinMax  = false;
    float maxChgVoltV  = 0;
    float minDsgVoltV  = 0;
    float maxChgCurrA  = 0;
    float maxDsgCurrA  = 0;
    bool fullChgReq    = false;
    uint8_t emergFlags = 0;
    ProtectParams protect;
};

// ---------------------------------------------------------------------------
// Alarm bits
// ---------------------------------------------------------------------------
#define PYTESRS485_ALARM_BITS(fnc) \
    fnc(OverVoltage,            (1<<0)) \
    fnc(UnderVoltage,           (1<<1)) \
    fnc(OverCurrentCharge,      (1<<2)) \
    fnc(OverCurrentDischarge,   (1<<3)) \
    fnc(OverTemperature,        (1<<4)) \
    fnc(UnderTemperature,       (1<<5)) \
    fnc(OverTemperatureCharge,  (1<<6)) \
    fnc(UnderTemperatureCharge, (1<<7)) \
    fnc(InternalFailure,        (1<<8)) \
    fnc(CellImbalance,          (1<<9))

enum class AlarmBits : uint16_t {
#define ALARM_ENUM(name, value) name = value,
    PYTESRS485_ALARM_BITS(ALARM_ENUM)
#undef ALARM_ENUM
};

static const frozen::map<AlarmBits, frozen::string, 10> AlarmBitTexts = {
#define ALARM_TEXT(name, value) { AlarmBits::name, #name },
    PYTESRS485_ALARM_BITS(ALARM_TEXT)
#undef ALARM_TEXT
};

// ---------------------------------------------------------------------------
// Warning bits
// ---------------------------------------------------------------------------
#define PYTESRS485_WARNING_BITS(fnc) \
    fnc(HighVoltage,            (1<<0)) \
    fnc(LowVoltage,             (1<<1)) \
    fnc(HighCurrentCharge,      (1<<2)) \
    fnc(HighCurrentDischarge,   (1<<3)) \
    fnc(HighTemperature,        (1<<4)) \
    fnc(LowTemperature,         (1<<5)) \
    fnc(HighTemperatureCharge,  (1<<6)) \
    fnc(LowTemperatureCharge,   (1<<7)) \
    fnc(InternalFailure,        (1<<8)) \
    fnc(CellImbalance,          (1<<9))

enum class WarningBits : uint16_t {
#define WARNING_ENUM(name, value) name = value,
    PYTESRS485_WARNING_BITS(WARNING_ENUM)
#undef WARNING_ENUM
};

static const frozen::map<WarningBits, frozen::string, 10> WarningBitTexts = {
#define WARNING_TEXT(name, value) { WarningBits::name, #name },
    PYTESRS485_WARNING_BITS(WARNING_TEXT)
#undef WARNING_TEXT
};

// ---------------------------------------------------------------------------
// DataPointLabel enum
// ---------------------------------------------------------------------------
enum class DataPointLabel : uint8_t {
    PackManufacturer                  = 0x01,
    BatteryVoltageMilliVolt           = 0x05,
    BatteryCurrentMilliAmps           = 0x06,
    BatterySoCPercent                 = 0x07,
    BatterySoHPercent                 = 0x08,
    TotalCapacityMilliAmpHours        = 0x09,
    RemainingCapacityMilliAmpHours    = 0x0A,
    CellMaxMilliVolt                  = 0x0B,
    CellMinMilliVolt                  = 0x0C,
    CellMaxTemperatureCelsius         = 0x0F,
    CellMinTemperatureCelsius         = 0x10,
    AccumulatedChargeDeciKWh          = 0x13,
    AccumulatedDischargeDeciKWh       = 0x14,
    ModuleCount                       = 0x15,
    AlarmsBitmask                     = 0x16,
    WarningsBitmask                   = 0x17,
    ChargeVoltageLimitMilliVolt       = 0x18,
    DischargeVoltageLimitMilliVolt    = 0x19,
    ChargeCurrentLimitMilliAmps       = 0x1A,
    DischargeCurrentLimitMilliAmps    = 0x1B,
    ChargeImmediately                 = 0x1C,
};

// ---------------------------------------------------------------------------
// Traits
// ---------------------------------------------------------------------------
template<DataPointLabel> struct DataPointLabelTraits;

#define LABEL_TRAIT(n, t, u) template<> struct DataPointLabelTraits<DataPointLabel::n> { \
    using type = t; \
    static constexpr char const name[] = #n; \
    static constexpr char const unit[] = u; \
};

LABEL_TRAIT(PackManufacturer,               std::string, "");
LABEL_TRAIT(BatteryVoltageMilliVolt,        uint32_t,    "mV");
LABEL_TRAIT(BatteryCurrentMilliAmps,        int32_t,     "mA");
LABEL_TRAIT(BatterySoCPercent,              float,       "%");
LABEL_TRAIT(BatterySoHPercent,              uint16_t,    "%");
LABEL_TRAIT(TotalCapacityMilliAmpHours,     uint32_t,    "mAh");
LABEL_TRAIT(RemainingCapacityMilliAmpHours, uint32_t,    "mAh");
LABEL_TRAIT(CellMaxMilliVolt,               uint16_t,    "mV");
LABEL_TRAIT(CellMinMilliVolt,               uint16_t,    "mV");
LABEL_TRAIT(CellMaxTemperatureCelsius,      int16_t,     "°C");
LABEL_TRAIT(CellMinTemperatureCelsius,      int16_t,     "°C");
LABEL_TRAIT(AccumulatedChargeDeciKWh,       uint32_t,    "kWh");
LABEL_TRAIT(AccumulatedDischargeDeciKWh,    uint32_t,    "kWh");
LABEL_TRAIT(ModuleCount,                    uint8_t,     "");
LABEL_TRAIT(AlarmsBitmask,                  uint16_t,    "");
LABEL_TRAIT(WarningsBitmask,                uint16_t,    "");
LABEL_TRAIT(ChargeVoltageLimitMilliVolt,    uint32_t,    "mV");
LABEL_TRAIT(DischargeVoltageLimitMilliVolt, uint32_t,    "mV");
LABEL_TRAIT(ChargeCurrentLimitMilliAmps,    uint32_t,    "mA");
LABEL_TRAIT(DischargeCurrentLimitMilliAmps, uint32_t,    "mA");
LABEL_TRAIT(ChargeImmediately,              bool,        "");
#undef LABEL_TRAIT

} // namespace Batteries::Pytes::Rs485

// ---------------------------------------------------------------------------
// DataPoint type alias and explicit template instantiation (outside namespace)
// ---------------------------------------------------------------------------
using PytesRs485DataPoint = DataPoint<bool, float, uint8_t, uint16_t, uint32_t, int16_t, int32_t, std::string>;

template class DataPointContainer<PytesRs485DataPoint, Batteries::Pytes::Rs485::DataPointLabel, Batteries::Pytes::Rs485::DataPointLabelTraits>;

namespace Batteries::Pytes::Rs485 {
    using DataPointContainer = ::DataPointContainer<PytesRs485DataPoint, DataPointLabel, DataPointLabelTraits>;
} // namespace Batteries::Pytes::Rs485
