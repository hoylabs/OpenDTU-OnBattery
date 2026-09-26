// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <vector>
#include <Arduino.h>

namespace Batteries::Pytes::Rs485 {

// Decodes and validates a received PYTES RS485 ASCII frame (protocol v1.21).
//
// The protocol wraps all values inside the INFO field as ASCII hex pairs, except
// for string fields which are raw ASCII bytes.  This class handles the frame-level
// validation (structure + checksum) and exposes the INFO payload via an iterator
// that callers advance by calling getU8/getU16/getU32/getS32/getString().
//
// The CID2 echo at the start of every INFO field is stripped and exposed
// separately via cid2(), so callers do not need to skip it themselves.
class SerialResponse {
public:
    using Iterator = std::vector<uint8_t>::const_iterator;

    explicit SerialResponse(std::vector<uint8_t> const& frame);

    bool    isValid()  const { return _valid; }
    uint8_t rtn()      const { return _rtn; }
    uint8_t cid2()     const { return _cid2; }
    size_t  infoSize() const { return _info.size(); }

    Iterator begin() const { return _info.cbegin(); }
    Iterator end()   const { return _info.cend(); }

    size_t remaining(Iterator const& pos) const {
        auto d = std::distance(pos, _info.cend());
        return d > 0 ? static_cast<size_t>(d) : 0;
    }

    // Advance pos and decode the next ASCII-hex-encoded numeric value.
    // Returns 0 (silently) on out-of-bounds or invalid hex — frame-level
    // validation is expected to catch structural problems before parsing.
    uint8_t  getU8(Iterator& pos)  const;
    uint16_t getU16(Iterator& pos) const;
    uint32_t getU32(Iterator& pos) const;
    int32_t  getS32(Iterator& pos) const;

    // Advance pos by n bytes and return them as a trimmed String.
    // String fields in this protocol are raw ASCII, NOT hex-encoded.
    String getString(Iterator& pos, size_t n) const;

private:
    static uint8_t  hexNibble(uint8_t c);
    static bool     decodeHexByte(uint8_t hi, uint8_t lo, uint8_t& out);
    static uint16_t frameChksum(std::vector<uint8_t> const& frame);

    uint32_t readHex(Iterator& pos, size_t nBytes) const;

    bool    _valid = false;
    uint8_t _rtn   = 0;
    uint8_t _cid2  = 0;
    std::vector<uint8_t> _info; // raw INFO bytes after the CID2 field
};

} // namespace Batteries::Pytes::Rs485
