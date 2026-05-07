// SPDX-License-Identifier: GPL-2.0-or-later
#include <battery/pytes/rs485/SerialCommand.h>

namespace Batteries::Pytes::Rs485 {

static constexpr uint8_t SOI = 0x7E;
static constexpr uint8_t EOI = 0x0D;
static constexpr uint8_t VER = 0x20;
static constexpr uint8_t CID1 = 0x46;

// Append two ASCII hex chars representing val to buf
static void appendHex8(std::vector<uint8_t>& buf, uint8_t val)
{
    static constexpr char hex[] = "0123456789ABCDEF";
    buf.push_back(hex[(val >> 4) & 0xF]);
    buf.push_back(hex[val & 0xF]);
}

static void appendHex16(std::vector<uint8_t>& buf, uint16_t val)
{
    appendHex8(buf, (val >> 8) & 0xFF);
    appendHex8(buf, val & 0xFF);
}

// LCHKSUM: sum of nibbles of LENID, mod 16, bitwise NOT + 1, masked to 4 bits
uint8_t SerialCommand::lchksum(uint16_t lenid)
{
    uint8_t d = ((lenid >> 8) & 0xF) + ((lenid >> 4) & 0xF) + (lenid & 0xF);
    return (~(d % 16) + 1) & 0xF;
}

// Frame CHKSUM: sum of all inner ASCII bytes mod 65536, two's complement
uint16_t SerialCommand::frameChksum(std::vector<uint8_t> const& inner)
{
    uint32_t sum = 0;
    for (uint8_t b : inner) { sum += b; }
    return static_cast<uint16_t>((~(sum % 65536) + 1) & 0xFFFF);
}

SerialCommand::SerialCommand(Command cmd, uint8_t addr, std::vector<uint8_t> info)
{
    // Build INFO as ASCII hex of raw bytes
    std::vector<uint8_t> infoAscii;
    for (uint8_t b : info) { appendHex8(infoAscii, b); }

    auto lenid = static_cast<uint16_t>(infoAscii.size());
    uint16_t lengthField = (static_cast<uint16_t>(lchksum(lenid)) << 12) | lenid;

    // Build inner (everything between SOI and CHKSUM/EOI)
    std::vector<uint8_t> inner;
    inner.reserve(8 + infoAscii.size());
    appendHex8(inner, VER);
    appendHex8(inner, addr);
    appendHex8(inner, CID1);
    appendHex8(inner, static_cast<uint8_t>(cmd));
    appendHex16(inner, lengthField);
    inner.insert(inner.end(), infoAscii.begin(), infoAscii.end());

    uint16_t chksum = frameChksum(inner);

    _frame.reserve(1 + inner.size() + 4 + 1);
    _frame.push_back(SOI);
    _frame.insert(_frame.end(), inner.begin(), inner.end());
    appendHex16(_frame, chksum);
    _frame.push_back(EOI);
}

} // namespace Batteries::Pytes::Rs485
