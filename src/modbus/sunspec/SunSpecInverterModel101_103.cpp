// SPDX-License-Identifier: GPL-2.0-or-later
// Spec: https://github.com/sunspec/models/blob/master/json/model_101.json
//       https://github.com/sunspec/models/blob/master/json/model_103.json
#include "modbus/sunspec/SunSpecInverterModel101_103.h"
#include "modbus/sunspec/SunSpecUtils.h"
#include <Hoymiles.h>

using SunSpec::NI_S;
using SunSpec::NI_U;
using SunSpec::SF_A;
using SunSpec::SF_HZ;
using SunSpec::SF_TMP;
using SunSpec::SF_V;
using SunSpec::SF_W;
using SunSpec::SF_WH;

uint16_t SunSpecInverterModel101_103::id(std::shared_ptr<InverterAbstract> const& inv)
{
    auto* stats = inv->Statistics();
    bool is3ph = stats->hasChannelFieldValue(TYPE_AC, CH0, FLD_IAC_1);
    return is3ph ? 103 : 101;
}

void SunSpecInverterModel101_103::fill(uint16_t* ac, std::shared_ptr<InverterAbstract> const& inv, uint8_t unitId)
{
    auto* stats = inv->Statistics();
    bool  is3ph = stats->hasChannelFieldValue(TYPE_AC, CH0, FLD_IAC_1);
    bool  reach = inv->isReachable();
    bool  prod  = inv->isProducing();

    float iac  = reach ? stats->getChannelFieldValue(TYPE_AC,  CH0, FLD_IAC) : 0.0f;
    float uac  = reach ? stats->getChannelFieldValue(TYPE_AC,  CH0, FLD_UAC) : 0.0f;
    float pac  = reach ? stats->getChannelFieldValue(TYPE_AC,  CH0, FLD_PAC) : 0.0f;
    float freq = reach ? stats->getChannelFieldValue(TYPE_AC,  CH0, FLD_F)   : 0.0f;
    float temp = reach ? stats->getChannelFieldValue(TYPE_INV, CH0, FLD_T)   : 0.0f;
    float yt   = reach ? stats->getChannelFieldValue(TYPE_INV, CH0, FLD_YT)  : 0.0f;
    uint32_t whTotal = static_cast<uint32_t>(yt * 1000.0f); // kWh -> Wh

    ac[0] = static_cast<uint16_t>(static_cast<int16_t>(iac * 100.0f + 0.5f)); // A: total AC current

    if (is3ph) {
        float i1 = reach ? stats->getChannelFieldValue(TYPE_AC, CH0, FLD_IAC_1) : 0.0f;
        float i2 = reach ? stats->getChannelFieldValue(TYPE_AC, CH0, FLD_IAC_2) : 0.0f;
        float i3 = reach ? stats->getChannelFieldValue(TYPE_AC, CH0, FLD_IAC_3) : 0.0f;
        ac[1] = static_cast<uint16_t>(static_cast<int16_t>(i1 * 100.0f + 0.5f)); // AphA
        ac[2] = static_cast<uint16_t>(static_cast<int16_t>(i2 * 100.0f + 0.5f)); // AphB
        ac[3] = static_cast<uint16_t>(static_cast<int16_t>(i3 * 100.0f + 0.5f)); // AphC
    } else {
        ac[1] = ac[0]; // AphA: total for single-phase
        ac[2] = NI_S; // AphB: not applicable, single-phase
        ac[3] = NI_S; // AphC: not applicable, single-phase
    }

    ac[4] = static_cast<uint16_t>(SF_A); // A_SF
    ac[5] = NI_U; // PPVphAB: phase-phase voltage not applicable
    ac[6] = NI_U; // PPVphBC: phase-phase voltage not applicable
    ac[7] = NI_U; // PPVphCA: phase-phase voltage not applicable

    if (is3ph) {
        float v1 = reach ? stats->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_1N) : 0.0f;
        float v2 = reach ? stats->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_2N) : 0.0f;
        float v3 = reach ? stats->getChannelFieldValue(TYPE_AC, CH0, FLD_UAC_3N) : 0.0f;
        ac[8] = static_cast<uint16_t>(static_cast<int16_t>(v1 * 10.0f + 0.5f)); // PhVphA
        ac[9] = static_cast<uint16_t>(static_cast<int16_t>(v2 * 10.0f + 0.5f)); // PhVphB
        ac[10] = static_cast<uint16_t>(static_cast<int16_t>(v3 * 10.0f + 0.5f)); // PhVphC
    } else {
        ac[8] = static_cast<uint16_t>(static_cast<int16_t>(uac * 10.0f + 0.5f)); // PhVphA: total for single-phase
        ac[9] = NI_U; // PhVphB: not applicable, single-phase
        ac[10] = NI_U; // PhVphC: not applicable, single-phase
    }

    ac[11] = static_cast<uint16_t>(SF_V); // V_SF
    ac[12] = static_cast<uint16_t>(static_cast<int16_t>(pac + 0.5f)); // W: AC power
    ac[13] = static_cast<uint16_t>(SF_W); // W_SF
    ac[14] = static_cast<uint16_t>(freq * 100.0f + 0.5f); // Hz
    ac[15] = static_cast<uint16_t>(SF_HZ); // Hz_SF
    ac[16] = NI_S; // VA: apparent power - not computed
    ac[17] = static_cast<uint16_t>(SF_W); // VA_SF
    ac[18] = NI_S; // VAr: reactive power - not computed
    ac[19] = static_cast<uint16_t>(SF_W); // VAr_SF
    ac[20] = NI_S; // PF: power factor - not computed
    ac[21] = static_cast<uint16_t>(SF_W); // PF_SF
    ac[22] = static_cast<uint16_t>(whTotal >> 16); // WH acc32 high word: lifetime energy
    ac[23] = static_cast<uint16_t>(whTotal & 0xFFFF); // WH acc32 low word
    ac[24] = static_cast<uint16_t>(SF_WH); // WH_SF
    ac[25] = NI_U; // DCA: DC current - not measured on the AC side
    ac[26] = static_cast<uint16_t>(SF_A); // DCA_SF
    ac[27] = NI_U; // DCV: DC voltage - not measured on the AC side
    ac[28] = static_cast<uint16_t>(SF_V); // DCV_SF
    ac[29] = NI_S; // DCW: DC power - not measured on the AC side
    ac[30] = static_cast<uint16_t>(SF_W); // DCW_SF

    bool hasTemp = reach && stats->hasChannelFieldValue(TYPE_INV, CH0, FLD_T);
    ac[31] = hasTemp ? static_cast<uint16_t>(static_cast<int16_t>(temp * 10.0f)) : NI_S; // TmpCab
    ac[32] = NI_S; // TmpSnk: heatsink temperature - not reported
    ac[33] = NI_S; // TmpTrns: transformer temperature - not reported
    ac[34] = NI_S; // TmpOt: other temperature - not reported
    ac[35] = static_cast<uint16_t>(SF_TMP); // Tmp_SF
    ac[36] = !reach ? 1 : (prod ? 4 : 8); // St: 1=Off, 4=MPPT, 8=Standby
    ac[37] = 0; // StVnd: vendor-specific status code - none defined, 0 = no code
    ac[38] = 0; // Evt1 hi: standard alarm bitfield, bits 0-15 - no per-bit alarm mapping, 0 = no active alarms
    ac[39] = 0; // Evt1 lo
    ac[40] = 0; // Evt2 hi: reserved for future SunSpec use, always 0
    ac[41] = 0; // Evt2 lo
    ac[42] = 0; // EvtVnd1 hi: vendor-defined alarm bitfield - none defined, 0 = none
    ac[43] = 0; // EvtVnd1 lo
    ac[44] = 0; // EvtVnd2 hi: vendor-defined alarm bitfield - none defined, 0 = none
    ac[45] = 0; // EvtVnd2 lo
    ac[46] = 0; // EvtVnd3 hi: vendor-defined alarm bitfield - none defined, 0 = none
    ac[47] = 0; // EvtVnd3 lo
    ac[48] = 0; // EvtVnd4 hi: vendor-defined alarm bitfield - none defined, 0 = none
    ac[49] = 0; // EvtVnd4 lo
}
