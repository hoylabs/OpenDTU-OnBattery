// SPDX-License-Identifier: GPL-2.0-or-later
// Spec: https://github.com/sunspec/models/blob/master/json/model_1.json
#include "modbus/sunspec/SunSpecCommonModel1.h"
#include "modbus/sunspec/SunSpecUtils.h"
#include <Hoymiles.h>
#include <cstdio>
#include <cstring>

void SunSpecCommonModel1::fill(uint16_t* payload, std::shared_ptr<InverterAbstract> const& inv, uint8_t unitId)
{
    // fw_build_version is MMmmpp: major*10000 + minor*100 + patch
    uint16_t fw = inv->DevInfo()->getFwBuildVersion();
    char version[16];
    snprintf(version, sizeof(version), "%u.%u.%u", fw / 10000, (fw / 100) % 100, fw % 100);

    SunSpec::packString(payload + 0, "OpenDTU", 16); // Mn: manufacturer
    SunSpec::packString(payload + 16, inv->typeName().c_str(), 16); // Md: model name
    memset(payload + 32, 0, 8 * sizeof(uint16_t)); // Opt: options - empty string, none defined
    SunSpec::packString(payload + 40, version, 8); // Vr: firmware version
    SunSpec::packString(payload + 48, inv->serialString().c_str(), 16); // SN: serial number
    payload[64] = (uint16_t)unitId; // DA: device address = unit ID for this inverter
}
