// SPDX-License-Identifier: GPL-2.0-or-later
#include "VictronMppt.h"
#include "MessageOutput.h"
#include "VictronSmartBatterySense.h"


void VictronSmartBatterySense::loop()
{
    // data update every second
    if ((millis() - _lastUpdate) < 1000) { return; }

    // if more MPPT are available, we use the fist MPPT with valid smart battery sense data
    auto idxMax = VictronMppt.controllerAmount();
    for (auto idx = 0; idx < idxMax; ++idx) {
        auto mpptData = VictronMppt.getData(idx);
        if ((mpptData->SmartBatterySenseTemperatureMilliCelsius.first != 0) && (VictronMppt.isDataValid(idx))) {
            _lastUpdate = millis() - VictronMppt.getDataAgeMillis(idx);
            _stats->updateFrom(mpptData->batteryVoltage_V_mV, mpptData->SmartBatterySenseTemperatureMilliCelsius.second, _lastUpdate);
            return;
        }
    }
}
