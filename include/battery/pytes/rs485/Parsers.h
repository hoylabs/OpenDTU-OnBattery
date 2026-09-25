// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <vector>
#include <battery/pytes/rs485/DataPoints.h>
#include <battery/pytes/rs485/SerialResponse.h>

namespace Batteries::Pytes::Rs485::Parsers {

// ---------------------------------------------------------------------------
// Battery-level parsers, the spec calls these "cluster" commands (CID2 0x60, 0x61, 0x62) — return DataPointContainer
// ---------------------------------------------------------------------------

// Parse 0x60 ClusterBasic.  Cluster serial number and module count (ModuleCount)
// are returned via the DataPointContainer.  Per-battery serials are skipped here
// as they are redundantly available from parseModuleBasic (0x80).
DataPointContainer parseClusterBasic(SerialResponse const& response);

DataPointContainer parseClusterAnalog(SerialResponse const& response);

DataPointContainer parseClusterChgDsg(SerialResponse const& response);

// ---------------------------------------------------------------------------
// Per-battery parsers (CID2 0x80, 0x81, 0x82, 0x83, 0x92) — return structs
// ---------------------------------------------------------------------------

struct ModuleBasicResult {
    uint8_t moduleNo = 0;
    uint8_t nCells   = 0;
    String hwVersion;
    String swVersion;
    String serial;
};

struct ModuleAnalogResult {
    uint8_t moduleNo           = 0;
    float voltageV             = 0;
    float currentA             = 0;
    float soc                  = 0;
    uint16_t health            = 0;
    uint32_t totalCapacityMah  = 0;
    uint32_t remainCapacityMah = 0;
    float ambientTemp          = 0;
    int chargeCycles           = -1;
    uint16_t balance           = 0;
    uint32_t status            = 0;
    uint32_t errorStatus       = 0;
    float cellMaxV             = 0;
    uint8_t cellMaxNo          = 0;
    float cellMinV             = 0;
    uint8_t cellMinNo          = 0;
    float tempMaxC             = 0;
    uint8_t tempMaxNo          = 0;
    float tempMinC             = 0;
    uint8_t tempMinNo          = 0;
};

struct ModuleChgDsgResult {
    uint8_t moduleNo    = 0;
    float maxChgVoltV   = 0;
    float minDsgVoltV   = 0;
    float maxChgCurrA   = 0;
    float maxDsgCurrA   = 0;
    bool chargeImmediately = false;
    bool fullChgReq     = false;
};

struct ModuleCellsResult {
    uint8_t moduleNo = 0;
    std::vector<CellData> cells;
};

ModuleBasicResult   parseModuleBasic(SerialResponse const& response);
ModuleAnalogResult  parseModuleAnalog(SerialResponse const& response);
ModuleChgDsgResult  parseModuleChgDsg(SerialResponse const& response);
ModuleCellsResult   parseModuleCells(SerialResponse const& response);

} // namespace Batteries::Pytes::Rs485::Parsers
