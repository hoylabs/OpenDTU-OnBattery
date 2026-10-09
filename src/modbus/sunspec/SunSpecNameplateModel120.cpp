// SPDX-License-Identifier: GPL-2.0-or-later
// Spec: https://github.com/sunspec/models/blob/master/json/model_120.json
// DTU-Pro: Technical Note - Modbus implementation using 3Gen DTU-Pro V1.2
//          https://www.mikrocontroller.net/attachment/552319/Technical-Note-Modbus-implementation-using-3Gen-DTU-Pro-V1.2.pdf
#include "modbus/sunspec/SunSpecNameplateModel120.h"
#include "modbus/sunspec/SunSpecUtils.h"
#include <Hoymiles.h>

using SunSpec::NI_S;
using SunSpec::NI_U;

void SunSpecNameplateModel120::fill(uint16_t* n, std::shared_ptr<InverterAbstract> const& inv, uint8_t unitId)
{
    n[0] = 4; // DERTyp: PV inverter
    n[1] = inv->DevInfo()->getMaxPower(); // WRtg: continuous power output capability (W)
    n[2] = 0; // WRtg_SF
    n[3] = NI_U; // VARtg: continuous VA capability - unknown, not published by DevInfo
    n[4] = NI_S; // VARtg_SF: paired with NI value, must also read NI
    n[5] = NI_S; // VArRtgQ1: continuous VAR capability, quadrant 1 - unknown
    n[6] = NI_S; // VArRtgQ2: continuous VAR capability, quadrant 2 - unknown
    n[7] = NI_S; // VArRtgQ3: continuous VAR capability, quadrant 3 - unknown
    n[8] = NI_S; // VArRtgQ4: continuous VAR capability, quadrant 4 - unknown
    n[9] = NI_S; // VArRtg_SF: paired with NI values, must also read NI
    n[10] = NI_U; // ARtg: max RMS AC current capability - unknown, not published by DevInfo
    n[11] = NI_S; // ARtg_SF: paired with NI value, must also read NI
    n[12] = NI_S; // PFRtgQ1: min power factor capability, quadrant 1 - unknown
    n[13] = NI_S; // PFRtgQ2: min power factor capability, quadrant 2 - unknown
    n[14] = NI_S; // PFRtgQ3: min power factor capability, quadrant 3 - unknown
    n[15] = NI_S; // PFRtgQ4: min power factor capability, quadrant 4 - unknown
    n[16] = NI_S; // PFRtg_SF: paired with NI values, must also read NI
    n[17] = 0; // WHRtg: nominal storage energy rating - genuinely 0, not a storage device
    n[18] = 0; // WHRtg_SF
    n[19] = 0; // AhrRtg: usable battery capacity - MUST be 0, some SunSpec clients (e.g. dbus-fronius) key off this to filter out battery/storage inverters
    n[20] = 0; // AhrRtg_SF: paired with AhrRtg, must also read as 0
    n[21] = 0; // MaxChaRte: max charge rate - genuinely 0, not a storage device
    n[22] = 0; // MaxChaRte_SF
    n[23] = 0; // MaxDisChaRte: max discharge rate - genuinely 0, not a storage device
    n[24] = 0; // MaxDisChaRte_SF
    n[25] = 0; // Pad: reserved, always 0
}
