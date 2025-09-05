// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <WiFiClient.h>
#include <Configuration.h>
#include <powermeter/Provider.h>

namespace PowerMeters::Modbus::Tcp {

class Provider : public ::PowerMeters::Provider {
public:
    explicit Provider(PowerMeterModbusTcpConfig const& cfg);
    ~Provider();

    bool init() final;
    void loop() final;
    bool isDataValid() const final;

private:
    static void pollingLoopHelper(void* context);
    bool readModbusRegister(const PowerMeterModbusTcpRegisterConfig& regConfig, float& value);
    bool connectToDevice();
    void pollingLoop();
    bool sendModbusRequest(uint8_t deviceId, uint8_t functionCode, uint16_t startRegister, uint16_t numRegisters);
    bool receiveModbusResponse(uint16_t* values, uint16_t numRegisters);

    PowerMeterModbusTcpConfig const _cfg;

    uint32_t _lastPoll = 0;
    std::atomic<bool> _taskDone;

    std::unique_ptr<WiFiClient> _upWiFiClient = nullptr;

    TaskHandle_t _taskHandle = nullptr;
    bool _stopPolling;
    mutable std::mutex _pollingMutex;
    std::condition_variable _cv;

    uint16_t _transactionId = 0;
};

} // namespace PowerMeters::Modbus::Tcp
