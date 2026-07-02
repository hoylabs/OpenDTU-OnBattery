// SPDX-License-Identifier: GPL-2.0-or-later
#include "modbus/sunspec/SunSpecUtils.h"
#include <cstring>

namespace SunSpec {

void packString(uint16_t* dst, const char* src, size_t numRegs)
{
    size_t srcLen = src ? strlen(src) : 0;
    for (size_t i = 0; i < numRegs; i++) {
        uint8_t hi = (i * 2     < srcLen) ? (uint8_t)src[i * 2]     : 0;
        uint8_t lo = (i * 2 + 1 < srcLen) ? (uint8_t)src[i * 2 + 1] : 0;
        dst[i] = ((uint16_t)hi << 8) | lo;
    }
}

} // namespace SunSpec
