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
    float currentA;     // A
    uint16_t soc;       // %
    uint32_t status;    // status bitmask, see spec 1.3.7
};

// ---------------------------------------------------------------------------
// Per-module aggregate info (stored in Stats::_modules)
// ---------------------------------------------------------------------------
struct BatteryModule {
    bool hasBasic   = false;
    bool hasAnalog  = false;
    bool hasCells   = false;
    bool hasChgDsg  = false;
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
    uint32_t status        = 0; // status bitmask, see spec 1.3.7
    uint32_t errorStatus   = 0; // system error bitmask, see spec 1.3.8
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
    uint32_t lastUpdate = 0; // millis() of the last 0x81 (analog) response
    float maxChgVoltV  = 0;
    float minDsgVoltV  = 0;
    float maxChgCurrA  = 0;
    float maxDsgCurrA  = 0;
    bool chargeImmediately = false; // emergency charge flag 1 or 2
    bool fullChgReq        = false;
};

// ---------------------------------------------------------------------------
// Alarm bits
// ---------------------------------------------------------------------------
// fnc(enum name, bit, key used for MQTT topic and live view)
#define PYTESRS485_ALARM_BITS(fnc) \
    fnc(OverVoltage,            (1<<0), "overVoltage") \
    fnc(UnderVoltage,           (1<<1), "underVoltage") \
    fnc(OverCurrentCharge,      (1<<2), "overCurrentCharge") \
    fnc(OverCurrentDischarge,   (1<<3), "overCurrentDischarge") \
    fnc(OverTemperature,        (1<<4), "overTemperature") \
    fnc(UnderTemperature,       (1<<5), "underTemperature") \
    fnc(OverTemperatureCharge,  (1<<6), "overTemperatureCharge") \
    fnc(UnderTemperatureCharge, (1<<7), "underTemperatureCharge") \
    fnc(InternalFailure,        (1<<8), "bmsInternal")

enum class AlarmBits : uint16_t {
#define ALARM_ENUM(name, value, key) name = value,
    PYTESRS485_ALARM_BITS(ALARM_ENUM)
#undef ALARM_ENUM
};

// ---------------------------------------------------------------------------
// Warning bits
// ---------------------------------------------------------------------------
#define PYTESRS485_WARNING_BITS(fnc) \
    fnc(HighVoltage,            (1<<0), "highVoltage") \
    fnc(LowVoltage,             (1<<1), "lowVoltage") \
    fnc(HighCurrentCharge,      (1<<2), "highCurrentCharge") \
    fnc(HighCurrentDischarge,   (1<<3), "highCurrentDischarge") \
    fnc(HighTemperature,        (1<<4), "highTemperature") \
    fnc(LowTemperature,         (1<<5), "lowTemperature") \
    fnc(HighTemperatureCharge,  (1<<6), "highTemperatureCharge") \
    fnc(LowTemperatureCharge,   (1<<7), "lowTemperatureCharge")

enum class WarningBits : uint16_t {
#define WARNING_ENUM(name, value, key) name = value,
    PYTESRS485_WARNING_BITS(WARNING_ENUM)
#undef WARNING_ENUM
};

// ---------------------------------------------------------------------------
// DataPointLabel enum
// ---------------------------------------------------------------------------
enum class DataPointLabel : uint8_t {
    Manufacturer                  = 0x01,
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
    StatusBitmask                 = 0x1F,
    ErrorBitmask                  = 0x20,
    FullChargeRequest                 = 0x25,
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

LABEL_TRAIT(Manufacturer,               std::string, "");
LABEL_TRAIT(BatteryVoltageMilliVolt,        uint32_t,    "mV");
LABEL_TRAIT(BatteryCurrentMilliAmps,        int32_t,     "mA");
LABEL_TRAIT(BatterySoCPercent,              float,       "%");
LABEL_TRAIT(BatterySoHPercent,              uint16_t,    "%");
LABEL_TRAIT(TotalCapacityMilliAmpHours,     uint32_t,    "mAh");
LABEL_TRAIT(RemainingCapacityMilliAmpHours, uint32_t,    "mAh");
LABEL_TRAIT(CellMaxMilliVolt,               uint16_t,    "mV");
LABEL_TRAIT(CellMinMilliVolt,               uint16_t,    "mV");
LABEL_TRAIT(CellMaxTemperatureCelsius,      float,       "°C");
LABEL_TRAIT(CellMinTemperatureCelsius,      float,       "°C");
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
LABEL_TRAIT(StatusBitmask,              uint32_t,    "");
LABEL_TRAIT(ErrorBitmask,               uint32_t,    "");
LABEL_TRAIT(FullChargeRequest,              bool,        "");
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
