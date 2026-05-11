// SPDX-License-Identifier: GPL-2.0-or-later
#include "HmsWifiProto.h"
#include <time.h>
#include <esp_log.h>

#undef TAG
static const char* TAG = "hmswifiproto";

// CRC-16/ARC: polynomial 0x8005 (reflected = 0xA001), init 0xFFFF, no final XOR.
// Same algorithm as the existing crc16() in the Hoymiles library but accepts
// size_t length to handle responses larger than 255 bytes.
static uint16_t hmsCrc16(const uint8_t* buf, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
            else crc >>= 1;
        }
    }
    return crc;
}

// ---------------------------------------------------------------------------
// Minimal protobuf helpers (wire format, no library dependency)
// ---------------------------------------------------------------------------

static void writeVarint(std::vector<uint8_t>& buf, uint64_t v)
{
    do {
        uint8_t b = v & 0x7F;
        v >>= 7;
        if (v) b |= 0x80;
        buf.push_back(b);
    } while (v);
}

// Read a varint from buf starting at pos; advances pos. Returns 0 on error.
static uint64_t readVarint(const uint8_t* buf, size_t& pos, size_t end)
{
    uint64_t result = 0;
    int shift = 0;
    while (pos < end) {
        uint8_t b = buf[pos++];
        result |= static_cast<uint64_t>(b & 0x7F) << shift;
        if (!(b & 0x80)) return result;
        shift += 7;
        if (shift >= 70) break; // malformed varint
    }
    return 0;
}

// Write a field tag (field_number << 3 | wire_type)
static void writeTag(std::vector<uint8_t>& buf, uint32_t fieldNum, uint8_t wireType)
{
    writeVarint(buf, (static_cast<uint64_t>(fieldNum) << 3) | wireType);
}

// Write a length-delimited field (bytes/string/embedded message)
static void writeLenDelim(std::vector<uint8_t>& buf, uint32_t fieldNum,
                           const uint8_t* data, size_t len)
{
    writeTag(buf, fieldNum, 2); // wire type 2 = length-delimited
    writeVarint(buf, len);
    for (size_t i = 0; i < len; i++) buf.push_back(data[i]);
}

// Write a varint (int32/int64/uint32) field
static void writeInt32Field(std::vector<uint8_t>& buf, uint32_t fieldNum, int32_t val)
{
    if (val == 0) return; // protobuf default – omit
    writeTag(buf, fieldNum, 0);
    // Negative int32 encoded as uint64 (sign-extended to 64 bits)
    writeVarint(buf, static_cast<uint64_t>(static_cast<int64_t>(val)));
}

static void writeInt64Field(std::vector<uint8_t>& buf, uint32_t fieldNum, int64_t val)
{
    if (val == 0) return;
    writeTag(buf, fieldNum, 0);
    writeVarint(buf, static_cast<uint64_t>(val));
}

// ---------------------------------------------------------------------------
// Frame builder
// ---------------------------------------------------------------------------

namespace HmsWifiProto {

std::vector<uint8_t> buildFrame(const uint8_t cmd[2], uint16_t seq,
                                 const std::vector<uint8_t>& payload)
{
    uint16_t crc = hmsCrc16(payload.data(), payload.size());

    uint16_t totalLen = static_cast<uint16_t>(payload.size()) + 10u;

    std::vector<uint8_t> frame;
    frame.reserve(totalLen);

    // Header: "HM" + command tag
    frame.push_back(0x48); // 'H'
    frame.push_back(0x4D); // 'M'
    frame.push_back(cmd[0]);
    frame.push_back(cmd[1]);

    // Sequence (big-endian)
    frame.push_back(static_cast<uint8_t>(seq >> 8));
    frame.push_back(static_cast<uint8_t>(seq & 0xFF));

    // CRC16 (big-endian)
    frame.push_back(static_cast<uint8_t>(crc >> 8));
    frame.push_back(static_cast<uint8_t>(crc & 0xFF));

    // Total length (big-endian)
    frame.push_back(static_cast<uint8_t>(totalLen >> 8));
    frame.push_back(static_cast<uint8_t>(totalLen & 0xFF));

    // Payload
    for (uint8_t b : payload) frame.push_back(b);

    return frame;
}

// ---------------------------------------------------------------------------
// Encode RealDataNewResDTO (trigger message sent to DTU)
//   time_ymd_hms = 1 (bytes)
//   cp            = 2 (int32)
//   offset        = 4 (int32)
//   time          = 5 (int32)
// ---------------------------------------------------------------------------
std::vector<uint8_t> encodeRealDataReq(int32_t cp)
{
    std::vector<uint8_t> buf;

    // Field 1: time_ymd_hms (bytes, "YYYY-MM-DD HH:MM:SS")
    time_t now;
    time(&now);
    struct tm t;
    gmtime_r(&now, &t);
    char timeStr[20];
    snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec);
    writeLenDelim(buf, 1, reinterpret_cast<const uint8_t*>(timeStr),
                  strlen(timeStr));

    // Field 2: cp
    writeInt32Field(buf, 2, cp);

    // Field 4: offset (28800 = 8 hours, used by Hoymiles cloud)
    writeInt32Field(buf, 4, HMS_WIFI_OFFSET);

    // Field 5: unix timestamp
    writeInt32Field(buf, 5, static_cast<int32_t>(now));

    return buf;
}

// ---------------------------------------------------------------------------
// Encode CommandResDTO (power limit command)
//   time       = 1 (int32)
//   action     = 2 (int32, 8 = CMD_ACTION_LIMIT_POWER)
//   package_nub = 4 (int32, 1)
//   tid        = 6 (int64)
//   data       = 7 (string, "A:{pct*10},B:0,C:0\r")
// ---------------------------------------------------------------------------
std::vector<uint8_t> encodePowerLimit(uint8_t pct)
{
    if (pct > 100) pct = 100;

    std::vector<uint8_t> buf;
    time_t now;
    time(&now);

    writeInt32Field(buf, 1, static_cast<int32_t>(now));
    writeInt32Field(buf, 2, HMS_CMD_LIMIT_POWER);
    writeInt32Field(buf, 4, 1); // package_nub
    writeInt64Field(buf, 6, static_cast<int64_t>(now)); // tid

    // data: "A:{pct*10},B:0,C:0\r"
    char dataStr[32];
    snprintf(dataStr, sizeof(dataStr), "A:%d,B:0,C:0\r",
             static_cast<int>(pct) * 10);
    writeLenDelim(buf, 7, reinterpret_cast<const uint8_t*>(dataStr),
                  strlen(dataStr));

    return buf;
}

// ---------------------------------------------------------------------------
// Encode CommandResDTO for MI start/stop (CMD_CLOUD_COMMAND_RES_DTO)
//   action 6 = CMD_ACTION_MI_START  (turn on)
//   action 7 = CMD_ACTION_MI_SHUTDOWN (turn off)
//   dev_kind 3 = 1 (DEV_DTU)
//   package_nub 4 = 1
//   tid 6 = unix timestamp
//   mi_to_sn 9 = inverter MI serial (repeated int64, one entry)
// ---------------------------------------------------------------------------
std::vector<uint8_t> encodePowerOnOff(bool on, uint64_t serial)
{
    std::vector<uint8_t> buf;
    time_t now;
    time(&now);

    writeInt32Field(buf, 2, on ? 6 : 7);            // action
    writeInt32Field(buf, 3, 1);                      // dev_kind = DEV_DTU
    writeInt32Field(buf, 4, 1);                      // package_nub
    writeInt64Field(buf, 6, static_cast<int64_t>(now)); // tid
    // mi_to_sn: repeated int64 field 9
    writeTag(buf, 9, 0);
    writeVarint(buf, serial);

    return buf;
}

// ---------------------------------------------------------------------------
// Encode CommandResDTO for DTU restart (CMD_CLOUD_COMMAND_RES_DTO)
//   action     = 2 (int32, 1 = CMD_ACTION_DTU_REBOOT)
//   package_nub = 4 (int32, 1)
//   tid        = 6 (int64, unix timestamp)
// ---------------------------------------------------------------------------
std::vector<uint8_t> encodeRestartDtu()
{
    std::vector<uint8_t> buf;
    time_t now;
    time(&now);

    writeInt32Field(buf, 2, 1); // action = CMD_ACTION_DTU_REBOOT
    writeInt32Field(buf, 4, 1); // package_nub
    writeInt64Field(buf, 6, static_cast<int64_t>(now)); // tid

    return buf;
}

// ---------------------------------------------------------------------------
// Protobuf decoder for RealDataNewReqDTO
// ---------------------------------------------------------------------------

static HmsWifiSgsmo parseSgsmo(const uint8_t* buf, size_t len)
{
    HmsWifiSgsmo s;
    size_t pos = 0;
    while (pos < len) {
        uint64_t tag = readVarint(buf, pos, len);
        uint32_t field = static_cast<uint32_t>(tag >> 3);
        uint8_t  wire  = static_cast<uint8_t>(tag & 7);
        if (wire == 0) {
            uint64_t raw = readVarint(buf, pos, len);
            switch (field) {
                case 1:  s.serial_number = static_cast<int64_t>(raw); break;
                case 3:  s.voltage       = static_cast<int32_t>(raw); break;
                case 4:  s.frequency     = static_cast<int32_t>(raw); break;
                case 5:  s.active_power  = static_cast<int32_t>(raw); break;
                case 6:  s.reactive_power = static_cast<int32_t>(raw); break;
                case 7:  s.current       = static_cast<int32_t>(raw); break;
                case 8:  s.power_factor  = static_cast<int32_t>(raw); break;
                case 9:  s.temperature   = static_cast<int32_t>(raw); break;
                default: break;
            }
        } else if (wire == 2) {
            uint64_t subLen = readVarint(buf, pos, len);
            pos += static_cast<size_t>(subLen); // skip unknown LEN fields
        } else if (wire == 5) {
            pos += 4; // 32-bit fixed
        } else if (wire == 1) {
            pos += 8; // 64-bit fixed
        }
    }
    return s;
}

static HmsWifiPvMo parsePvMo(const uint8_t* buf, size_t len)
{
    HmsWifiPvMo p;
    size_t pos = 0;
    while (pos < len) {
        uint64_t tag = readVarint(buf, pos, len);
        uint32_t field = static_cast<uint32_t>(tag >> 3);
        uint8_t  wire  = static_cast<uint8_t>(tag & 7);
        if (wire == 0) {
            uint64_t raw = readVarint(buf, pos, len);
            switch (field) {
                case 1:  p.serial_number = static_cast<int64_t>(raw); break;
                case 2:  p.port_number   = static_cast<int32_t>(raw); break;
                case 3:  p.voltage       = static_cast<int32_t>(raw); break;
                case 4:  p.current       = static_cast<int32_t>(raw); break;
                case 5:  p.power         = static_cast<int32_t>(raw); break;
                case 6:  p.energy_total  = static_cast<int32_t>(raw); break;
                case 7:  p.energy_daily  = static_cast<int32_t>(raw); break;
                default: break;
            }
        } else if (wire == 2) {
            uint64_t subLen = readVarint(buf, pos, len);
            pos += static_cast<size_t>(subLen);
        } else if (wire == 5) {
            pos += 4;
        } else if (wire == 1) {
            pos += 8;
        }
    }
    return p;
}

HmsWifiRealData parseRealDataFrame(const std::vector<uint8_t>& frame)
{
    HmsWifiRealData result;

    ESP_LOGI(TAG, "parseRealDataFrame: %zu bytes", frame.size());

    if (frame.size() < 11) {
        ESP_LOGW(TAG, "frame too short: %zu", frame.size());
        return result;
    }

    if (frame[0] != 0x48 || frame[1] != 0x4D) {
        ESP_LOGW(TAG, "bad header: %02X %02X", frame[0], frame[1]);
        return result;
    }

    uint16_t totalLen = (static_cast<uint16_t>(frame[8]) << 8) | frame[9];
    ESP_LOGI(TAG, "totalLen=%u frame.size=%zu", totalLen, frame.size());

    if (frame.size() < totalLen) {
        ESP_LOGW(TAG, "frame truncated: have %zu need %u", frame.size(), totalLen);
        return result;
    }

    uint16_t expectedCrc = (static_cast<uint16_t>(frame[6]) << 8) | frame[7];
    uint16_t computedCrc = hmsCrc16(frame.data() + 10, totalLen - 10);
    ESP_LOGI(TAG, "CRC: expected=0x%04X computed=0x%04X", expectedCrc, computedCrc);

    if (computedCrc != expectedCrc) {
        ESP_LOGW(TAG, "CRC mismatch – check frame layout");
        return result;
    }

    const uint8_t* buf = frame.data() + 10;
    size_t len = totalLen - 10;
    size_t pos = 0;

    while (pos < len) {
        uint64_t tag = readVarint(buf, pos, len);
        if (tag == 0 && pos >= len) break;
        uint32_t field = static_cast<uint32_t>(tag >> 3);
        uint8_t  wire  = static_cast<uint8_t>(tag & 7);

        ESP_LOGD(TAG, "  field=%u wire=%u pos=%zu", field, wire, pos);

        if (wire == 0) {
            uint64_t raw = readVarint(buf, pos, len);
            if (field == 3) result.ap = static_cast<int32_t>(raw);
            else if (field == 4) result.cp = static_cast<int32_t>(raw);
        } else if (wire == 2) {
            uint64_t subLen = readVarint(buf, pos, len);
            size_t subStart = pos;
            pos += static_cast<size_t>(subLen);
            if (pos > len) break;

            if (field == 9) {
                ESP_LOGD(TAG, "  -> SGSMO (%llu bytes)", subLen);
                result.sgs_data.push_back(parseSgsmo(buf + subStart,
                                                      static_cast<size_t>(subLen)));
            } else if (field == 11) {
                ESP_LOGD(TAG, "  -> PvMO (%llu bytes)", subLen);
                result.pv_data.push_back(parsePvMo(buf + subStart,
                                                    static_cast<size_t>(subLen)));
            }
        } else if (wire == 5) {
            pos += 4;
        } else if (wire == 1) {
            pos += 8;
        } else {
            ESP_LOGW(TAG, "  unknown wiretype %u at pos %zu – stopping", wire, pos);
            break;
        }
    }

    result.valid = !result.sgs_data.empty() || !result.pv_data.empty();
    ESP_LOGI(TAG, "parse done: valid=%d sgs=%zu pv=%zu ap=%d cp=%d",
             result.valid, result.sgs_data.size(), result.pv_data.size(),
             result.ap, result.cp);
    return result;
}

// ---------------------------------------------------------------------------
// Encode APPInfoDataResDTO (trigger to pull firmware/hardware info)
//   time_ymd_hms = 1 (bytes, "YYYY-MM-DD HH:MM:SS")
//   offset        = 2 (int32, 28800)
//   time          = 5 (uint32, unix timestamp)
// ---------------------------------------------------------------------------
std::vector<uint8_t> encodeAppInfoReq()
{
    std::vector<uint8_t> buf;
    time_t now;
    time(&now);
    struct tm t;
    gmtime_r(&now, &t);
    char timeStr[20];
    snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec);
    writeLenDelim(buf, 1, reinterpret_cast<const uint8_t*>(timeStr), strlen(timeStr));
    writeInt32Field(buf, 2, HMS_WIFI_OFFSET);
    writeInt32Field(buf, 5, static_cast<int32_t>(now));
    return buf;
}

// ---------------------------------------------------------------------------
// Parse APPInfoDataReqDTO response.
// Extracts the APPPvInfoMO entry (field 11) whose pv_serial_number matches
// `serial`; falls back to the first PV entry if none match.
// ---------------------------------------------------------------------------
// Parse one APPPvInfoMO submessage; also returns the pv_serial_number for matching.
struct PvInfoEntry {
    uint64_t pv_serial            = 0;
    int32_t  pv_sw_version        = 0;
    int32_t  pv_hw_part_num       = 0;
    int32_t  pv_hw_version        = 0;
    int32_t  pv_grid_profile_code = 0;
    int32_t  pv_grid_profile      = 0;
};

static PvInfoEntry parsePvInfoMo(const uint8_t* buf, size_t len)
{
    PvInfoEntry e;
    size_t pos = 0;
    while (pos < len) {
        uint64_t tag   = readVarint(buf, pos, len);
        uint32_t field = static_cast<uint32_t>(tag >> 3);
        uint8_t  wire  = static_cast<uint8_t>(tag & 7);
        if (wire == 0) {
            uint64_t raw = readVarint(buf, pos, len);
            switch (field) {
                case 2:  e.pv_serial            = raw;                         break;
                case 4:  e.pv_sw_version        = static_cast<int32_t>(raw);  break;
                case 5:  e.pv_hw_part_num       = static_cast<int32_t>(raw);  break;
                case 6:  e.pv_hw_version        = static_cast<int32_t>(raw);  break;
                case 7:  e.pv_grid_profile_code = static_cast<int32_t>(raw);  break;
                case 8:  e.pv_grid_profile      = static_cast<int32_t>(raw);  break;
                default: break;
            }
        } else if (wire == 2) {
            uint64_t subLen = readVarint(buf, pos, len);
            pos += static_cast<size_t>(subLen);
        } else if (wire == 5) { pos += 4;
        } else if (wire == 1) { pos += 8;
        } else break;
    }
    return e;
}

HmsAppInfoResult parseAppInfoFrame(const std::vector<uint8_t>& frame, uint64_t serial)
{
    HmsAppInfoResult result;

    if (frame.size() < 11 || frame[0] != 0x48 || frame[1] != 0x4D) {
        ESP_LOGW(TAG, "app-info: bad frame");
        return result;
    }

    uint16_t totalLen = (static_cast<uint16_t>(frame[8]) << 8) | frame[9];
    if (frame.size() < totalLen) return result;

    uint16_t expectedCrc = (static_cast<uint16_t>(frame[6]) << 8) | frame[7];
    if (hmsCrc16(frame.data() + 10, totalLen - 10) != expectedCrc) {
        ESP_LOGW(TAG, "app-info: CRC mismatch");
        return result;
    }

    const uint8_t* buf = frame.data() + 10;
    size_t len = totalLen - 10;
    size_t pos = 0;

    // Collect all PV entries; prefer the one matching our serial.
    PvInfoEntry first;
    PvInfoEntry matched;
    bool foundFirst = false;
    bool foundMatch = false;

    while (pos < len) {
        uint64_t tag   = readVarint(buf, pos, len);
        if (tag == 0 && pos >= len) break;
        uint32_t field = static_cast<uint32_t>(tag >> 3);
        uint8_t  wire  = static_cast<uint8_t>(tag & 7);

        if (wire == 0) {
            readVarint(buf, pos, len);
        } else if (wire == 2) {
            uint64_t subLen = readVarint(buf, pos, len);
            size_t subStart = pos;
            pos += static_cast<size_t>(subLen);
            if (pos > len) break;

            if (field == 11) { // APPPvInfoMO
                PvInfoEntry e = parsePvInfoMo(buf + subStart, static_cast<size_t>(subLen));
                if (!foundFirst) { first = e; foundFirst = true; }
                if (e.pv_serial == serial) { matched = e; foundMatch = true; }
            }
        } else if (wire == 5) { pos += 4;
        } else if (wire == 1) { pos += 8;
        } else break;
    }

    const PvInfoEntry& e = foundMatch ? matched : first;
    result.pv_sw_version        = e.pv_sw_version;
    result.pv_hw_part_number    = e.pv_hw_part_num;
    result.pv_hw_version        = e.pv_hw_version;
    result.pv_grid_profile_code = e.pv_grid_profile_code;
    result.pv_grid_profile      = e.pv_grid_profile;
    result.valid                = foundFirst;
    ESP_LOGI(TAG, "app-info: sw=%d hwpn=0x%08X hwv=%d gpf_code=0x%04X gpf=0x%04X serial_match=%d",
             result.pv_sw_version, static_cast<uint32_t>(result.pv_hw_part_number),
             result.pv_hw_version, static_cast<uint32_t>(result.pv_grid_profile_code),
             static_cast<uint32_t>(result.pv_grid_profile), foundMatch);
    return result;
}

// ---------------------------------------------------------------------------
// Encode GetConfigResDTO (query sent to DTU to fetch device configuration)
//   offset = 1 (int32, 28800 = 8 h)
//   time   = 2 (uint32, unix timestamp - 60)
// ---------------------------------------------------------------------------
std::vector<uint8_t> encodeGetConfigReq()
{
    std::vector<uint8_t> buf;
    time_t now;
    time(&now);

    writeInt32Field(buf, 1, HMS_WIFI_OFFSET);
    writeInt32Field(buf, 2, static_cast<int32_t>(now - 60));

    return buf;
}

// ---------------------------------------------------------------------------
// Parse GetConfigReqDTO response (inverter replies with current config).
// We only care about field 5 = limit_power_mypower (percent * 10).
// ---------------------------------------------------------------------------
HmsGetConfigResult parseGetConfigFrame(const std::vector<uint8_t>& frame)
{
    HmsGetConfigResult result;

    if (frame.size() < 11) {
        ESP_LOGW(TAG, "get-config: frame too short: %zu", frame.size());
        return result;
    }

    if (frame[0] != 0x48 || frame[1] != 0x4D) {
        ESP_LOGW(TAG, "get-config: bad header");
        return result;
    }

    uint16_t totalLen = (static_cast<uint16_t>(frame[8]) << 8) | frame[9];
    if (frame.size() < totalLen) {
        ESP_LOGW(TAG, "get-config: truncated: have %zu need %u", frame.size(), totalLen);
        return result;
    }

    uint16_t expectedCrc = (static_cast<uint16_t>(frame[6]) << 8) | frame[7];
    uint16_t computedCrc = hmsCrc16(frame.data() + 10, totalLen - 10);
    if (computedCrc != expectedCrc) {
        ESP_LOGW(TAG, "get-config: CRC mismatch 0x%04X vs 0x%04X", computedCrc, expectedCrc);
        return result;
    }

    const uint8_t* buf = frame.data() + 10;
    size_t len = totalLen - 10;
    size_t pos = 0;

    while (pos < len) {
        uint64_t tag = readVarint(buf, pos, len);
        if (tag == 0 && pos >= len) break;
        uint32_t field = static_cast<uint32_t>(tag >> 3);
        uint8_t  wire  = static_cast<uint8_t>(tag & 7);

        if (wire == 0) {
            uint64_t raw = readVarint(buf, pos, len);
            if (field == 5) {
                result.limit_power_mypower = static_cast<int32_t>(raw);
            }
        } else if (wire == 2) {
            uint64_t subLen = readVarint(buf, pos, len);
            pos += static_cast<size_t>(subLen);
        } else if (wire == 5) {
            pos += 4;
        } else if (wire == 1) {
            pos += 8;
        } else {
            break;
        }
    }

    result.valid = (result.limit_power_mypower >= 0);
    ESP_LOGI(TAG, "get-config: limit_power_mypower=%d", result.limit_power_mypower);
    return result;
}

} // namespace HmsWifiProto
