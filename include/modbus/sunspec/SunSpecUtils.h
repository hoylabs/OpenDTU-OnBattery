// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>

// Model definitions (JSON): https://github.com/sunspec/models/tree/master/json
namespace SunSpec {

// Scale factors (SunSpec sunssf: signed int16, power-of-10 exponent)
static constexpr int16_t SF_A   = -2; // current:        raw = A   * 100
static constexpr int16_t SF_V   = -1; // voltage:        raw = V   * 10
static constexpr int16_t SF_W   =  0; // power:          raw = W
static constexpr int16_t SF_VAR =  0; // reactive power: raw = VAr
static constexpr int16_t SF_PF  = -2; // power factor:   raw = PF  * 100
static constexpr int16_t SF_HZ  = -2; // freq:           raw = Hz  * 100
static constexpr int16_t SF_WH  =  0; // energy:         raw = Wh
static constexpr int16_t SF_TMP = -1; // temp:           raw = °C  * 10

// SunSpec "not implemented" sentinels
static constexpr uint16_t NI_U = 0xFFFF; // uint16
static constexpr uint16_t NI_S = 0x8000; // int16

// ASCII string packed 2 chars/register, null-padded.
void packString(uint16_t* dst, const char* src, size_t numRegs);

} // namespace SunSpec
