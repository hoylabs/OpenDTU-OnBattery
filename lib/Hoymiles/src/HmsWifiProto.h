// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <Arduino.h>
#include <cstdint>
#include <vector>

// HMS WiFi Protocol Constants
#define HMS_WIFI_PORT          10081
#define HMS_WIFI_TIMEOUT_MS    5000
#define HMS_WIFI_OFFSET        28800
#define HMS_CMD_LIMIT_POWER    8

// Command tags (big-endian 2 bytes)
// Client sends CMD_REAL_RES_DTO to trigger data push from DTU
static constexpr uint8_t HMS_CMD_REAL_RES[2]    = { 0xA3, 0x11 };
// Client sends CMD_COMMAND_RES_DTO to set power limit / on/off
static constexpr uint8_t HMS_CMD_COMMAND_RES[2] = { 0xA3, 0x05 };
// Client sends CMD_GET_CONFIG to query inverter configuration (incl. limit)
static constexpr uint8_t HMS_CMD_GET_CONFIG[2]  = { 0xA3, 0x09 };
// Client sends CMD_CLOUD_COMMAND_RES_DTO for MI start/stop commands
static constexpr uint8_t HMS_CMD_CLOUD_COMMAND[2] = { 0x23, 0x05 };
// Client sends CMD_APP_INFO_DATA_RES_DTO to request firmware/hardware info
static constexpr uint8_t HMS_CMD_APP_INFO[2]      = { 0xA3, 0x01 };

// Decoded single-phase inverter block (SGSMO)
struct HmsWifiSgsmo {
    int64_t serial_number = 0;
    int32_t voltage       = 0; // raw / 10 = V
    int32_t frequency     = 0; // raw / 100 = Hz
    int32_t active_power  = 0; // raw / 10 = W
    int32_t reactive_power = 0; // raw / 10 = VAR (signed)
    int32_t current       = 0; // raw / 100 = A
    int32_t power_factor  = 0; // raw / 1000
    int32_t temperature   = 0; // raw / 10 = °C (signed)
};

// Decoded PV port block (PvMO)
struct HmsWifiPvMo {
    int64_t serial_number = 0;
    int32_t port_number   = 0; // 1-based
    int32_t voltage       = 0; // raw / 10 = V
    int32_t current       = 0; // raw / 100 = A
    int32_t power         = 0; // raw / 10 = W
    int32_t energy_total  = 0; // raw / 1000 = kWh
    int32_t energy_daily  = 0; // raw = Wh
};

struct HmsAppInfoResult {
    int32_t pv_sw_version        = 0;
    int32_t pv_hw_part_number    = 0;
    int32_t pv_hw_version        = 0;
    int32_t pv_grid_profile_code = 0; // profile type ID (lIdx in low byte, hIdx in high byte)
    int32_t pv_grid_profile      = 0; // profile version (low byte = version byte, high = sub)
    bool valid = false;
};

struct HmsGetConfigResult {
    int32_t limit_power_mypower = -1; // percent * 10 (1000 = 100%); -1 = not present
    bool valid = false;
};

struct HmsWifiRealData {
    int32_t ap = 0;  // total pages from DTU
    int32_t cp = 0;  // current page
    std::vector<HmsWifiSgsmo> sgs_data;
    std::vector<HmsWifiPvMo>  pv_data;
    bool valid = false;
};

namespace HmsWifiProto {

// Build a complete framed message for the HMS WiFi protocol.
// cmd must be a 2-byte command tag (e.g. HMS_CMD_REAL_RES).
// seq is incremented and embedded in the frame.
std::vector<uint8_t> buildFrame(const uint8_t cmd[2], uint16_t seq,
                                 const std::vector<uint8_t>& payload);

// Encode RealDataNewResDTO as protobuf (trigger to get inverter data).
std::vector<uint8_t> encodeRealDataReq(int32_t cp);

// Encode CommandResDTO for a power limit command (pct: 0-100).
std::vector<uint8_t> encodePowerLimit(uint8_t pct);

// Encode CommandResDTO for inverter on/off.
// serial: inverter MI serial number (written to mi_to_sn field).
std::vector<uint8_t> encodePowerOnOff(bool on, uint64_t serial);

// Encode CommandResDTO for DTU restart (action=1, CMD_CLOUD_COMMAND_RES_DTO).
std::vector<uint8_t> encodeRestartDtu();

// Parse a RealDataNewReqDTO response from a framed response buffer.
// Returns parsed data; .valid == false on error.
HmsWifiRealData parseRealDataFrame(const std::vector<uint8_t>& frame);

// Encode APPInfoDataResDTO (trigger to get firmware/hardware info from DTU).
std::vector<uint8_t> encodeAppInfoReq();

// Parse APPInfoDataReqDTO response; picks the PV entry matching `serial`.
// Falls back to the first entry if no match. .valid == false on error.
HmsAppInfoResult parseAppInfoFrame(const std::vector<uint8_t>& frame, uint64_t serial);

// Encode GetConfigResDTO (request sent to DTU to fetch device config).
std::vector<uint8_t> encodeGetConfigReq();

// Parse a GetConfigReqDTO response; .valid == false on error.
HmsGetConfigResult parseGetConfigFrame(const std::vector<uint8_t>& frame);

} // namespace HmsWifiProto
