// SPDX-License-Identifier: GPL-2.0-or-later
#include <solarcharger/integrated/Provider.h>
#include <Configuration.h>
#include <LogHelper.h>

static const char* TAG = "solarCharger";
static const char* SUBTAG = "integrated";

namespace SolarChargers::Integrated {

bool Provider::init()
{
    if (Configuration.get().Battery.Provider != 7) {
        DTU_LOGE("Init failed - you must use a supported battery integration. Currently supported integrations are: Zendure");
        return false;
    }

    return true;
}

} // namespace SolarChargers::Integrated
