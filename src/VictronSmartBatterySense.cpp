// SPDX-License-Identifier: GPL-2.0-or-later

#include "VictronMppt.h"
#include "MessageOutput.h"
#include "VictronSmartBatterySense.h"

void VictronSmartBatterySense::deinit()
{
    // nothing to deinitialize, we use the MPPT interface
    ;
}

bool VictronSmartBatterySense::init(bool verboseLogging)
{
    // we use the existing Victron MPPT interface
    if (VictronMppt.controllerAmount() > 0) {
        MessageOutput.println("[VictronSmartBatterySense] Use existing MPPT interface...");
        return true;
    } else {
        MessageOutput.println("[VictronSmartBatterySense] No MPPT interface available...");
        return false;
    }
}

void VictronSmartBatterySense::loop()
{
    // data update every second
    if ((millis() - _lastUpdate) < 1000) { return; }

    // if more MPPT are available, we use the fist MPPT with valid smart battery sense data
    uint32_t volt {0};
    int32_t temp {0};
    auto idxMax = VictronMppt.controllerAmount();
    for (auto idx = 0; idx < idxMax; ++idx) {
        auto mpptData = VictronMppt.getData(idx);
        if ((mpptData->SmartBatterySenseTemperatureMilliCelsius.first != 0) && (VictronMppt.isDataValid(idx))) {
            volt = mpptData->batteryVoltage_V_mV;
            temp = mpptData->SmartBatterySenseTemperatureMilliCelsius.second;
            _lastUpdate = VictronMppt.getDataAgeMillis(idx) + millis();
            break;
        }
    }

     _stats->updateFrom(volt, temp, _lastUpdate);
}
