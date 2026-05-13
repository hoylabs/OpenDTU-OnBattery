// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "InverterAbstract.h"
#include "../HmsWifiProto.h"
#include <IPAddress.h>

// HMS-xxxxW-2T WiFi inverter support.
// These inverters have a built-in DTU that exposes a TCP server on port 10081.
// Communication uses Protocol Buffers over TCP instead of RF radio.
// Supports 1- and 2-MPPT variants: the class holds 2 DC channel slots and
// fills only those that the inverter reports.
class HMS_2T : public InverterAbstract {
public:
    explicit HMS_2T(HoymilesRadio* radio, const uint64_t serial);

    void setWifiIp(const IPAddress& ip);
    const IPAddress& getWifiIp() const { return _wifiIp; }

    String typeName() const override;
    const byteAssign_t* getByteAssignment() const override;
    uint8_t getByteAssignmentSize() const override;
    const channelMetaData_t* getChannelMetaData() const override;
    uint8_t getChannelMetaDataSize() const override;

    bool sendStatsRequest() override;
    bool sendAlarmLogRequest(bool force = false) override { return false; }
    bool sendDevInfoRequest() override;  // fetches firmware/hardware info via APPInfomationData
    bool sendSystemConfigParaRequest() override;
    bool sendActivePowerControlRequest(float limit, PowerLimitControlType type) override;
    bool resendActivePowerControlRequest() override;
    bool sendPowerControlRequest(bool turnOn) override;
    bool resendPowerControlRequest() override;
    bool sendRestartControlRequest() override;
    bool sendChangeChannelRequest() override { return false; }
    bool sendGridOnProFileParaRequest() override;
    bool supportsPowerDistributionLogic() override { return false; }
    uint32_t getEffectivePollIntervalSecs() const override { return kMinPollIntervalMs / 1000U; }

private:
    IPAddress _wifiIp;
    uint16_t  _seq = 0;

    // Last commanded state (for resend support)
    float _lastLimitPct = 100.0f;
    bool  _lastPowerOn  = true;

    // WiFi inverters should not be polled more often than once per 30 s.
    uint32_t _lastStatsMs = 0;
    static constexpr uint32_t kMinPollIntervalMs = 34000;

    // Grid profile codes cached from the last sendDevInfoRequest().
    // Set when devinfo is fetched; consumed by sendGridOnProFileParaRequest().
    int32_t _gridProfileCode    = 0;
    int32_t _gridProfileVersion = 0;

    uint16_t nextSeq() { return ++_seq; }

    // Open a TCP connection, send msg, read response, close.
    // Returns empty vector on timeout / connection error.
    std::vector<uint8_t> sendWifiRequest(const std::vector<uint8_t>& msg);

    // Open a TCP connection, send msg, close immediately without reading.
    // Use for fire-and-forget commands (on/off, restart) where waiting for
    // a response would block the lwIP core lock and starve async_tcp.
    bool sendWifiCommand(const std::vector<uint8_t>& msg);

    // Write decoded real-time data into the StatisticsParser buffer.
    void applyRealData(const HmsWifiRealData& data);

    bool doSetPowerLimit(float pct);
    bool doSetPowerState(bool on);
    uint16_t getConfiguredMaxPowerWatts();
};
