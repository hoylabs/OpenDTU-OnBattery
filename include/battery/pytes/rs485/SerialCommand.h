// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <Arduino.h>
#include <vector>

namespace Batteries::Pytes::Rs485 {

// Builds a valid PYTES RS485 ASCII frame (protocol v1.21).
// Frame layout: SOI(1) + VER(2) + ADR(2) + CID1(2) + CID2(2) +
//               LENGTH(4) + INFO(LENID chars) + CHKSUM(4) + EOI(1)
// All fields except SOI and EOI are ASCII hex encoded.
class SerialCommand {
public:
    // CID2 command codes
    enum class Command : uint8_t {
        PackBasic    = 0x60,
        PackAnalog   = 0x61,
        PackChgDsg   = 0x62,
        ModuleBasic    = 0x80,
        ModuleAnalog   = 0x81,
        ModuleProtect  = 0x82,
        ModuleChgDsg   = 0x83,
        ModuleCells    = 0x92,
    };

    // addr: cluster address (1-based)
    // info: optional INFO payload bytes (already the raw data, will be hex-encoded)
    explicit SerialCommand(Command cmd, uint8_t addr = 1, std::vector<uint8_t> info = {});

    uint8_t const* data() const { return _frame.data(); }
    size_t         size() const { return _frame.size(); }

private:
    std::vector<uint8_t> _frame;

    static uint8_t  lchksum(uint16_t lenid);
    static uint16_t frameChksum(std::vector<uint8_t> const& inner);
};

} // namespace Batteries::Pytes::Rs485
