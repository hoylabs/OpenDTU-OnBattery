// SPDX-License-Identifier: GPL-2.0-or-later
#include <battery/pytes/rs485/SerialResponse.h>
#include <LogHelper.h>

#undef TAG
static const char* TAG = "battery";
static const char* SUBTAG = "Pytes RS485";

namespace Batteries::Pytes::Rs485 {

static constexpr uint8_t SOI = 0x7E;
static constexpr uint8_t EOI = 0x0D;

uint8_t SerialResponse::hexNibble(uint8_t c)
{
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    return 0xFF;
}

bool SerialResponse::decodeHexByte(uint8_t hi, uint8_t lo, uint8_t& out)
{
    uint8_t h = hexNibble(hi), l = hexNibble(lo);
    if (h == 0xFF || l == 0xFF) { return false; }
    out = (h << 4) | l;
    return true;
}

// Sum all inner ASCII bytes (between SOI and the 4-char CHKSUM+EOI), two's complement mod 65536
uint16_t SerialResponse::frameChksum(std::vector<uint8_t> const& frame)
{
    uint32_t sum = 0;
    for (size_t i = 1; i + 5 < frame.size(); ++i) { sum += frame[i]; }
    return static_cast<uint16_t>((~(sum % 65536) + 1) & 0xFFFF);
}

SerialResponse::SerialResponse(std::vector<uint8_t> const& frame)
{
    // SOI(1) + VER(2)+ADR(2)+CID1(2)+RTN(2)+LEN(4) + CHKSUM(4) + EOI(1) = 18 bytes minimum
    if (frame.size() < 18 || frame.front() != SOI || frame.back() != EOI) {
        DTU_LOGW("Malformed frame (size=%u)", static_cast<unsigned>(frame.size()));
        return;
    }

    // Verify checksum (warn only — some BMSes have known CS quirks)
    uint8_t csHi, csLo;
    size_t n = frame.size();
    if (!decodeHexByte(frame[n-5], frame[n-4], csHi) ||
        !decodeHexByte(frame[n-3], frame[n-2], csLo)) {
        DTU_LOGW("Bad checksum hex encoding");
        return;
    }
    uint16_t csRecv = (static_cast<uint16_t>(csHi) << 8) | csLo;
    if (csRecv != frameChksum(frame)) {
        DTU_LOGW("Checksum mismatch: recv=0x%04X calc=0x%04X", csRecv, frameChksum(frame));
    }

    // Decode fixed 12-char ASCII header: VER(2) ADR(2) CID1(2) RTN(2) LEN(4)
    uint8_t rtn, lenHi, lenLo;
    if (!decodeHexByte(frame[7], frame[8], rtn)    ||
        !decodeHexByte(frame[9], frame[10], lenHi)  ||
        !decodeHexByte(frame[11], frame[12], lenLo)) {
        DTU_LOGW("Failed to decode frame header");
        return;
    }
    _rtn = rtn;
    uint16_t lenid = ((static_cast<uint16_t>(lenHi) << 8) | lenLo) & 0x0FFF;

    // Validate INFO bounds
    constexpr size_t infoOffset = 13; // SOI(1) + header(12)
    if (infoOffset + lenid + 5 > n) {
        DTU_LOGW("INFO length %u out of bounds (frame %u bytes)", lenid, static_cast<unsigned>(n));
        return;
    }

    // Decode CID2 (first 2 ASCII chars of INFO)
    if (lenid < 2) {
        DTU_LOGW("INFO too short for CID2");
        return;
    }
    uint8_t cid2;
    if (!decodeHexByte(frame[infoOffset], frame[infoOffset + 1], cid2)) {
        DTU_LOGW("Failed to decode CID2");
        return;
    }
    _cid2 = cid2;

    // Store the remaining INFO bytes (after the 2-char CID2 field)
    _info.assign(frame.cbegin() + infoOffset + 2,
                 frame.cbegin() + infoOffset + lenid);

    _valid = true;
}

uint32_t SerialResponse::readHex(Iterator& pos, size_t nBytes) const
{
    size_t chars = nBytes * 2;
    if (std::distance(pos, _info.cend()) < static_cast<ptrdiff_t>(chars)) { return 0; }
    uint32_t val = 0;
    for (size_t i = 0; i < chars; ++i) {
        val = (val << 4) | (hexNibble(*(pos++)) & 0xF);
    }
    return val;
}

uint8_t  SerialResponse::getU8(Iterator& pos)  const { return static_cast<uint8_t>(readHex(pos, 1)); }
uint16_t SerialResponse::getU16(Iterator& pos) const { return static_cast<uint16_t>(readHex(pos, 2)); }
uint32_t SerialResponse::getU32(Iterator& pos) const { return readHex(pos, 4); }

int32_t SerialResponse::getS32(Iterator& pos) const
{
    uint32_t v = readHex(pos, 4);
    return v >= 0x80000000u ? static_cast<int32_t>(v - 0x100000000ull) : static_cast<int32_t>(v);
}

String SerialResponse::getString(Iterator& pos, size_t n) const
{
    size_t avail = static_cast<size_t>(std::distance(pos, _info.cend()));
    if (avail < n) { return ""; }
    String s;
    s.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        char c = static_cast<char>(*(pos++));
        if (c != '\0') { s += c; }
    }
    s.trim();
    return s;
}

} // namespace Batteries::Pytes::Rs485
