// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <optional>
#include <stdint.h>

class BatteryClass {
public:
    // mandatory values for all battery interfaces
    uint16_t stateOfCharge;
    uint32_t stateOfChargeLastUpdate;
    uint32_t lastUpdate; // the last time *any* datum was updated
    std::string manufacturer;

    // common values
    float chargeCurrentLimitation;
    float dischargeCurrentLimitation;
    float voltage; // total voltage of the battery pack
    // total current into (positive) or from (negative)
    // the battery, i.e., the charging current
    float current;
    float temperature;
    bool chargeEnabled;
    bool dischargeEnabled;

    bool alarmOverCurrentDischarge;
    bool alarmUnderTemperature;
    bool alarmOverTemperature;
    bool alarmUnderVoltage;
    bool alarmOverVoltage;
    bool alarmBmsInternal;
    bool alarmOverCurrentCharge;

    bool warningHighCurrentDischarge;
    bool warningLowTemperature;
    bool warningHighTemperature;
    bool warningLowVoltage;
    bool warningHighVoltage;
    bool warningBmsInternal;
    bool warningHighCurrentCharge;

    uint16_t stateOfHealth;
    float chargeVoltage;
    bool chargeImmediately;

private:
};

extern BatteryClass Battery;
