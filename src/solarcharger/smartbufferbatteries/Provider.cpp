// SPDX-License-Identifier: GPL-2.0-or-later
#include <solarcharger/smartbufferbatteries/Provider.h>
#include <Configuration.h>
#include <MessageOutput.h>
#include <battery/Controller.h>
#include <battery/zendure/Stats.h>

namespace SolarChargers::SmartBufferBatteries {

bool Provider::init(bool verboseLogging)
{
    _verboseLogging = verboseLogging;

    if (Configuration.get().Battery.Provider != 7) {
        MessageOutput.printf("[SolarChargers::SmartBufferBatteries]: Init failed - must use SmartBufferBattery!");
        return false;
    }

    return true;
}

} // namespace SolarChargers::SmartBufferBatteries
