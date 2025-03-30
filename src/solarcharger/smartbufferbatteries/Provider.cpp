// SPDX-License-Identifier: GPL-2.0-or-later
#include <solarcharger/smartbufferbatteries/Provider.h>
#include <Configuration.h>
#include <battery/Controller.h>
#include <battery/zendure/Stats.h>
#include <LogHelper.h>

static const char* TAG = "solarCharger";
static const char* SUBTAG = "smartBufferBatteries";

namespace SolarChargers::SmartBufferBatteries {

bool Provider::init()
{
    if (Configuration.get().Battery.Provider != 7) {
        DTU_LOGE("Init failed - must use SmartBufferBattery!");
        return false;
    }

    return true;
}

} // namespace SolarChargers::SmartBufferBatteries
