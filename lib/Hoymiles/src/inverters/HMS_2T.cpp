// SPDX-License-Identifier: GPL-2.0-or-later
#include "HMS_2T.h"
#include "../HmsWifiProto.h"
#include "../parser/GridProfileParser.h"
#include "HoymilesRadio.h"
#include <WiFiClient.h>
#include <WiFi.h>
#include <Arduino.h>
#include <esp_log.h>

#undef TAG
static const char* TAG = "hms2t";

// ---------------------------------------------------------------------------
// Byte assignment table - two DC channels (CH0 = MPPT A, CH1 = MPPT B).
// All start offsets >= 2.  StatisticsParser::setChannelFieldValue() uses a
// uint8_t loop counter that wraps to 255 when start==0, causing an infinite
// loop and heap corruption.  RF inverter tables use the same convention.
// ---------------------------------------------------------------------------
static const byteAssign_t byteAssignment[] = {
    // DC CH0 (PV port 1)
    { TYPE_DC, CH0, FLD_UDC,  UNIT_V,    2,  2, 10,   false, 1 },
    { TYPE_DC, CH0, FLD_IDC,  UNIT_A,    4,  2, 100,  false, 2 },
    { TYPE_DC, CH0, FLD_PDC,  UNIT_W,    6,  2, 10,   false, 1 },
    { TYPE_DC, CH0, FLD_YD,   UNIT_WH,   8,  2, 1,    false, 0 },
    { TYPE_DC, CH0, FLD_YT,   UNIT_KWH,  10, 4, 1000, false, 3 },
    { TYPE_DC, CH0, FLD_IRR,  UNIT_PCT,  CALC_CH_IRR, CH0, CMD_CALC, false, 3 },

    // DC CH1 (PV port 2 - stays at 0 for 1-MPPT inverters)
    { TYPE_DC, CH1, FLD_UDC,  UNIT_V,    14, 2, 10,   false, 1 },
    { TYPE_DC, CH1, FLD_IDC,  UNIT_A,    16, 2, 100,  false, 2 },
    { TYPE_DC, CH1, FLD_PDC,  UNIT_W,    18, 2, 10,   false, 1 },
    { TYPE_DC, CH1, FLD_YD,   UNIT_WH,   20, 2, 1,    false, 0 },
    { TYPE_DC, CH1, FLD_YT,   UNIT_KWH,  22, 4, 1000, false, 3 },
    { TYPE_DC, CH1, FLD_IRR,  UNIT_PCT,  CALC_CH_IRR, CH1, CMD_CALC, false, 3 },

    // AC CH0
    { TYPE_AC, CH0, FLD_UAC,  UNIT_V,    26, 2, 10,   false, 1 },
    { TYPE_AC, CH0, FLD_IAC,  UNIT_A,    28, 2, 100,  false, 2 },
    { TYPE_AC, CH0, FLD_PAC,  UNIT_W,    30, 2, 10,   false, 1 },
    { TYPE_AC, CH0, FLD_Q,    UNIT_VAR,  32, 2, 10,   true,  1 },
    { TYPE_AC, CH0, FLD_F,    UNIT_HZ,   34, 2, 100,  false, 2 },
    { TYPE_AC, CH0, FLD_PF,   UNIT_NONE, 36, 2, 1000, false, 3 },

    // INV CH0
    { TYPE_INV, CH0, FLD_T,       UNIT_C,    38, 2, 10, true,  1 },
    { TYPE_INV, CH0, FLD_EVT_LOG, UNIT_NONE, 40, 2, 1,  false, 0 },

    // Calculated aggregate fields
    { TYPE_INV, CH0, FLD_YD,  UNIT_WH,  CALC_TOTAL_YD,  0, CMD_CALC, false, 0 },
    { TYPE_INV, CH0, FLD_YT,  UNIT_KWH, CALC_TOTAL_YT,  0, CMD_CALC, false, 3 },
    { TYPE_INV, CH0, FLD_PDC, UNIT_W,   CALC_TOTAL_PDC, 0, CMD_CALC, false, 1 },
    { TYPE_INV, CH0, FLD_EFF, UNIT_PCT, CALC_TOTAL_EFF, 0, CMD_CALC, false, 3 }
};

static const channelMetaData_t channelMetaData[] = {
    { CH0, MPPT_A },
    { CH1, MPPT_B }
};

// Buffer size: highest offset + num bytes (FLD_EVT_LOG: start=40, num=2 -> 42).
static constexpr uint8_t kStatsByteCount = 42;

// ---------------------------------------------------------------------------
HMS_2T::HMS_2T(HoymilesRadio* radio, const uint64_t serial)
    : InverterAbstract(radio, serial)
{
    // We cannot query the limit from the inverter, so assume 100% until a
    // command is sent (which calls setLimitPercent with the actual value).
    SystemConfigPara()->setLimitPercent(100.0f);
}

void HMS_2T::setWifiIp(const IPAddress& ip)
{
    _wifiIp = ip;
}

String HMS_2T::typeName() const
{
    return "HMS-xxxxW-2T (WiFi)";
}

const byteAssign_t* HMS_2T::getByteAssignment() const
{
    return byteAssignment;
}

uint8_t HMS_2T::getByteAssignmentSize() const
{
    return sizeof(byteAssignment) / sizeof(byteAssignment[0]);
}

const channelMetaData_t* HMS_2T::getChannelMetaData() const
{
    return channelMetaData;
}

uint8_t HMS_2T::getChannelMetaDataSize() const
{
    return sizeof(channelMetaData) / sizeof(channelMetaData[0]);
}

// ---------------------------------------------------------------------------
// Grid profile
// ---------------------------------------------------------------------------

bool HMS_2T::sendGridOnProFileParaRequest()
{
    // Grid profile codes come from the APPPvInfoMO in the app-info response,
    // which is fetched by sendDevInfoRequest().  If devinfo hasn't run yet
    // those fields are still zero — return false so the loop retries.
    if (_gridProfileCode == 0) return false;

    // Build a minimal 7-byte profile header.
    // containsValidData() requires _gridProfileLength > 6.
    // Byte layout mirrors the RF GridOnProFilePara payload:
    //   [0] = lIdx (profile type low byte)
    //   [1] = hIdx (profile type high byte)
    //   [2] = version byte  (high nibble = major, low nibble = minor)
    //   [3] = version sub byte
    // Detailed section/value data is not available via the WiFi app-info
    // protocol — only the profile identifier is exposed.
    // 7 bytes: 4-byte profile header + 0xFF sentinel so the section parser
    // immediately hits an unknown ID and returns an empty section list.
    uint8_t buf[7] = { 0, 0, 0, 0, 0xFF, 0, 0 };
    buf[0] = static_cast<uint8_t>((_gridProfileCode >> 8) & 0xFF); // lIdx
    buf[1] = static_cast<uint8_t>(_gridProfileCode & 0xFF);        // hIdx
    buf[2] = static_cast<uint8_t>((_gridProfileVersion >> 8) & 0xFF); // version byte
    buf[3] = static_cast<uint8_t>(_gridProfileVersion & 0xFF);         // version sub

    GridProfile()->clearBuffer();
    GridProfile()->appendFragment(0, buf, sizeof(buf));
    GridProfile()->setLastUpdate(millis());

    ESP_LOGI(TAG, "%s: grid profile: %s %s",
             serialString().c_str(),
             GridProfile()->getProfileName().c_str(),
             GridProfile()->getProfileVersion().c_str());
    return true;
}

// ---------------------------------------------------------------------------
// TCP communication
// ---------------------------------------------------------------------------

std::vector<uint8_t> HMS_2T::sendWifiRequest(const std::vector<uint8_t>& msg)
{
    WiFiClient client;

    if (!client.connect(_wifiIp, HMS_WIFI_PORT, HMS_WIFI_TIMEOUT_MS)) {
        ESP_LOGW(TAG, "Cannot connect to %s:%d", _wifiIp.toString().c_str(), HMS_WIFI_PORT);
        return {};
    }

    client.write(msg.data(), msg.size());

    // Wait for first byte (up to HMS_WIFI_TIMEOUT_MS)
    uint32_t deadline = millis() + HMS_WIFI_TIMEOUT_MS;
    while (client.available() == 0 && millis() < deadline) {
        delay(5);
    }

    // Read until no new bytes arrive for 50 ms (handles multi-segment TCP responses)
    std::vector<uint8_t> response;
    uint32_t idleDeadline = millis() + 50;
    while (millis() < idleDeadline) {
        while (client.available()) {
            response.push_back(static_cast<uint8_t>(client.read()));
            idleDeadline = millis() + 50;
        }
        delay(2);
    }

    client.stop();
    ESP_LOGI(TAG, "sendWifiRequest: received %zu bytes", response.size());
    return response;
}

bool HMS_2T::sendWifiCommand(const std::vector<uint8_t>& msg)
{
    WiFiClient client;
    if (!client.connect(_wifiIp, HMS_WIFI_PORT, HMS_WIFI_TIMEOUT_MS)) {
        ESP_LOGW(TAG, "sendWifiCommand: connect to %s:%d failed",
                 _wifiIp.toString().c_str(), HMS_WIFI_PORT);
        return false;
    }
    client.write(msg.data(), msg.size());
    client.stop();
    return true;
}

// ---------------------------------------------------------------------------
// Stats request (periodic data polling)
// ---------------------------------------------------------------------------

bool HMS_2T::sendStatsRequest()
{
    if (!getEnablePolling()) return false;

    // Hoymiles cloud rate-limits at 30 s; the inverter itself dislikes faster polls.
    uint32_t now = millis();
    if (_lastStatsMs != 0 && now - _lastStatsMs < kMinPollIntervalMs) {
        return true;
    }
    _lastStatsMs = now;

    ESP_LOGI(TAG, "%s: polling %s:%d", serialString().c_str(),
             _wifiIp.toString().c_str(), HMS_WIFI_PORT);

    // We may need to fetch multiple pages (ap > 1).  Start with cp=0.
    HmsWifiRealData combined;
    RadioStats.TxRequestData++;

    for (int32_t cp = 0; ; cp++) {
        auto payload = HmsWifiProto::encodeRealDataReq(cp);
        auto frame   = HmsWifiProto::buildFrame(HMS_CMD_REAL_RES, nextSeq(), payload);
        auto resp    = sendWifiRequest(frame);

        auto data = HmsWifiProto::parseRealDataFrame(resp);
        if (!data.valid) {
            if (cp == 0) {
                ESP_LOGW(TAG, "%s: stats request failed (page 0)", serialString().c_str());
                RadioStats.RxFailNoAnswer++;
                Statistics()->incrementRxFailureCount();
                return false;
            }
            break; // partial success - use what we have
        }

        // Merge into combined result
        for (auto& s : data.sgs_data) combined.sgs_data.push_back(s);
        for (auto& p : data.pv_data)  combined.pv_data.push_back(p);
        combined.valid = true;

        if (cp == 0) combined.ap = data.ap;
        if (cp >= combined.ap - 1) break; // got all pages
    }

    if (!combined.valid) {
        RadioStats.RxFailNoAnswer++;
        Statistics()->incrementRxFailureCount();
        return false;
    }

    RadioStats.RxSuccess++;
    setLastRssi(static_cast<int8_t>(WiFi.RSSI()));
    applyRealData(combined);
    Statistics()->resetRxFailureCount();
    return true;
}

// ---------------------------------------------------------------------------
// Apply decoded data to the StatisticsParser buffer.
// Uses appendFragment() to set _statisticLength (needed for offset logic),
// then overwrites values via setChannelFieldValue().
// ---------------------------------------------------------------------------
void HMS_2T::applyRealData(const HmsWifiRealData& data)
{
    // Zero the buffer so stale data from previous cycles is cleared,
    // then set the buffer length so the parser knows data is present.
    static const uint8_t zeroBuf[kStatsByteCount] = {};
    Statistics()->clearBuffer();
    Statistics()->appendFragment(0, zeroBuf, kStatsByteCount);

    // AC / temperature from first SGSMO entry
    if (!data.sgs_data.empty()) {
        const auto& s = data.sgs_data[0];
        Statistics()->setChannelFieldValue(TYPE_AC, CH0, FLD_UAC, s.voltage / 10.0f);
        Statistics()->setChannelFieldValue(TYPE_AC, CH0, FLD_F,   s.frequency / 100.0f);
        Statistics()->setChannelFieldValue(TYPE_AC, CH0, FLD_PAC, s.active_power / 10.0f);
        Statistics()->setChannelFieldValue(TYPE_AC, CH0, FLD_Q,   s.reactive_power / 10.0f);
        Statistics()->setChannelFieldValue(TYPE_AC, CH0, FLD_IAC, s.current / 100.0f);
        Statistics()->setChannelFieldValue(TYPE_AC, CH0, FLD_PF,  s.power_factor / 1000.0f);
        Statistics()->setChannelFieldValue(TYPE_INV, CH0, FLD_T,  s.temperature / 10.0f);
    }

    // DC data from PvMO entries (port_number is 1-based)
    for (const auto& p : data.pv_data) {
        // Map port 1 -> CH0, port 2 -> CH1
        ChannelNum_t ch = (p.port_number <= 1) ? CH0 : CH1;
        Statistics()->setChannelFieldValue(TYPE_DC, ch, FLD_UDC, p.voltage / 10.0f);
        Statistics()->setChannelFieldValue(TYPE_DC, ch, FLD_IDC, p.current / 100.0f);
        Statistics()->setChannelFieldValue(TYPE_DC, ch, FLD_PDC, p.power / 10.0f);
        Statistics()->setChannelFieldValue(TYPE_DC, ch, FLD_YD,  static_cast<float>(p.energy_daily));
        Statistics()->setChannelFieldValue(TYPE_DC, ch, FLD_YT,  p.energy_total / 1000.0f);
    }

    Statistics()->setLastUpdate(millis());
}

// ---------------------------------------------------------------------------
// DevInfo - populate minimal info so the UI shows something useful
// ---------------------------------------------------------------------------

bool HMS_2T::sendDevInfoRequest()
{
    auto payload = HmsWifiProto::encodeAppInfoReq();
    auto frame   = HmsWifiProto::buildFrame(HMS_CMD_APP_INFO, nextSeq(), payload);
    auto resp    = sendWifiRequest(frame);

    auto info = HmsWifiProto::parseAppInfoFrame(resp, serial());
    if (!info.valid) {
        ESP_LOGW(TAG, "%s: app-info request failed", serialString().c_str());
        return false;
    }

    // Populate DevInfoAll buffer layout:
    //   [0-1] FW build version (pv_sw_version)
    //   [2-3] build year  (0x07E4 = 2020, satisfies containsValidData() year > 2016)
    //   [4-5] build month/day encoded as mm*100+dd (0x0065 = 101 = Jan 01)
    //   [6-7] build hour/minute encoded as hh*100+mm (zero)
    //   [8-9] bootloader version (zero – not available via WiFi protocol)
    uint8_t allBuf[10] = {};
    allBuf[0] = static_cast<uint8_t>(info.pv_sw_version >> 8);
    allBuf[1] = static_cast<uint8_t>(info.pv_sw_version & 0xFF);
    allBuf[2] = 0x07; // year high byte: 0x07E4 = 2020
    allBuf[3] = 0xE4;
    allBuf[4] = 0x00; // month/day: 0x0065 = 101 → Jan 1  (mm*100+dd)
    allBuf[5] = 0x65;
    DevInfo()->clearBufferAll();
    DevInfo()->appendFragmentAll(0, allBuf, sizeof(allBuf));
    DevInfo()->setLastUpdateAll(millis());

    // Populate DevInfoSimple buffer layout:
    //   [0-1] FW build version, [2-5] HW part number (big-endian), [6-7] HW version bytes.
    uint8_t simpleBuf[8] = {};
    simpleBuf[0] = static_cast<uint8_t>(info.pv_sw_version >> 8);
    simpleBuf[1] = static_cast<uint8_t>(info.pv_sw_version & 0xFF);
    const uint32_t hwpn = static_cast<uint32_t>(info.pv_hw_part_number);
    simpleBuf[2] = static_cast<uint8_t>(hwpn >> 24);
    simpleBuf[3] = static_cast<uint8_t>(hwpn >> 16);
    simpleBuf[4] = static_cast<uint8_t>(hwpn >> 8);
    simpleBuf[5] = static_cast<uint8_t>(hwpn & 0xFF);
    simpleBuf[6] = static_cast<uint8_t>(info.pv_hw_version >> 8);
    simpleBuf[7] = static_cast<uint8_t>(info.pv_hw_version & 0xFF);
    DevInfo()->clearBufferSimple();
    DevInfo()->appendFragmentSimple(0, simpleBuf, sizeof(simpleBuf));
    DevInfo()->setLastUpdateSimple(millis());

    _gridProfileCode    = info.pv_grid_profile_code;
    _gridProfileVersion = info.pv_grid_profile;

    ESP_LOGI(TAG, "%s: dev info: model=%s maxPower=%uW hwpn=0x%08X gpf_code=0x%04X gpf=0x%04X",
             serialString().c_str(),
             DevInfo()->getHwModelName().c_str(),
             DevInfo()->getMaxPower(),
             static_cast<uint32_t>(info.pv_hw_part_number),
             static_cast<uint32_t>(_gridProfileCode),
             static_cast<uint32_t>(_gridProfileVersion));
    return true;
}

// ---------------------------------------------------------------------------
// System config (reads current power limit from the inverter)
// ---------------------------------------------------------------------------

bool HMS_2T::sendSystemConfigParaRequest()
{
    auto payload = HmsWifiProto::encodeGetConfigReq();
    auto frame   = HmsWifiProto::buildFrame(HMS_CMD_GET_CONFIG, nextSeq(), payload);
    auto resp    = sendWifiRequest(frame);

    auto result  = HmsWifiProto::parseGetConfigFrame(resp);
    if (!result.valid) {
        ESP_LOGW(TAG, "%s: get-config failed", serialString().c_str());
        return false;
    }

    // limit_power_mypower is percent * 10 (1000 = 100%)
    float pct = static_cast<float>(result.limit_power_mypower) / 10.0f;
    SystemConfigPara()->setLimitPercent(pct);
    SystemConfigPara()->setLastUpdateRequest(millis());
    ESP_LOGI(TAG, "%s: limit read back = %.1f%%", serialString().c_str(), pct);
    return true;
}

// ---------------------------------------------------------------------------
// Power limit
// ---------------------------------------------------------------------------

bool HMS_2T::doSetPowerLimit(float pct)
{
    if (pct < 0.0f)   pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;

    auto payload = HmsWifiProto::encodePowerLimit(static_cast<uint8_t>(pct));
    auto frame   = HmsWifiProto::buildFrame(HMS_CMD_COMMAND_RES, nextSeq(), payload);
    bool ok = sendWifiCommand(frame);

    if (ok) {
        _lastLimitPct = pct;
        SystemConfigPara()->setLastLimitCommandSuccess(CMD_OK);
        SystemConfigPara()->setLastUpdateCommand(millis());
        SystemConfigPara()->setLimitPercent(pct);
    } else {
        SystemConfigPara()->setLastLimitCommandSuccess(CMD_NOK);
        ESP_LOGW(TAG, "%s: set power limit failed", serialString().c_str());
    }
    return ok;
}

bool HMS_2T::sendActivePowerControlRequest(float limit, PowerLimitControlType type)
{
    if (!getEnableCommands()) return false;

    float pct = limit;
    if (type == PowerLimitControlType::AbsolutNonPersistent
            || type == PowerLimitControlType::AbsolutPersistent) {
        // Convert watts to percent using the configured upper power limit.
        // If not known, send at 100%.
        float maxW = static_cast<float>(getConfiguredMaxPowerWatts());
        pct = (maxW > 0.0f) ? (limit / maxW * 100.0f) : 100.0f;
    }

    SystemConfigPara()->setLastUpdateCommand(millis());
    return doSetPowerLimit(pct);
}

bool HMS_2T::resendActivePowerControlRequest()
{
    if (!getEnableCommands()) return false;
    return doSetPowerLimit(_lastLimitPct);
}

// ---------------------------------------------------------------------------
// Power on/off
// ---------------------------------------------------------------------------

bool HMS_2T::doSetPowerState(bool on)
{
    auto payload = HmsWifiProto::encodePowerOnOff(on, serial());
    auto frame   = HmsWifiProto::buildFrame(HMS_CMD_CLOUD_COMMAND, nextSeq(), payload);
    bool ok = sendWifiCommand(frame);

    if (ok) {
        _lastPowerOn = on;
        PowerCommand()->setLastPowerCommandSuccess(CMD_OK);
        PowerCommand()->setLastUpdateCommand(millis());
    } else {
        PowerCommand()->setLastPowerCommandSuccess(CMD_NOK);
        ESP_LOGW(TAG, "%s: set power state failed", serialString().c_str());
    }
    return ok;
}

bool HMS_2T::sendRestartControlRequest()
{
    if (!getEnableCommands()) return false;

    auto payload = HmsWifiProto::encodeRestartDtu();
    auto frame   = HmsWifiProto::buildFrame(HMS_CMD_CLOUD_COMMAND, nextSeq(), payload);
    bool ok = sendWifiCommand(frame);
    if (ok) ESP_LOGI(TAG, "%s: restart-dtu sent", serialString().c_str());
    return ok;
}

bool HMS_2T::sendPowerControlRequest(bool turnOn)
{
    if (!getEnableCommands()) return false;
    PowerCommand()->setLastUpdateCommand(millis());
    return doSetPowerState(turnOn);
}

bool HMS_2T::resendPowerControlRequest()
{
    if (!getEnableCommands()) return false;
    return doSetPowerState(_lastPowerOn);
}

// ---------------------------------------------------------------------------
// Helper used by sendActivePowerControlRequest for abs -> % conversion.
// Walks channel config to find configured upper power limit.
// ---------------------------------------------------------------------------
uint16_t HMS_2T::getConfiguredMaxPowerWatts()
{
    // Sum up MaxChannelPower across all DC channels as a proxy for peak watts.
    uint16_t total = 0;
    for (auto ch : getChannelsDC()) {
        total += Statistics()->getStringMaxPower(static_cast<uint8_t>(ch));
    }
    return (total > 0) ? total : 800; // fallback: 800 W
}
