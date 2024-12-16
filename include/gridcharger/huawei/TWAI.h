// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <FreeRTOS.h>
#include <freertos/task.h>
#include <cstdint>
#include <array>
#include <mutex>
#include <map>
#include <queue>
#include <driver/twai.h>
#include <gridcharger/huawei/HardwareInterface.h>

namespace GridCharger::Huawei {

// Error codes
#define HUAWEI_ERROR_CODE_RX 0x01
#define HUAWEI_ERROR_CODE_TX 0x02

// Updateinterval used to request new values from the PSU
#define HUAWEI_DATA_REQUEST_INTERVAL_MS 2500

class TWAI : public HardwareInterface {
public:
    ~TWAI();

    bool init() final;
    uint8_t getErrorCode(bool clear) final;

    HardwareInterface::property_t getParameter(HardwareInterface::Property property) final;

    void setParameter(HardwareInterface::Setting setting, float val) final;

private:
    static void staticLoopHelper(void* context);
    void loopHelper();
    void loop();
    bool receiveMessage(twai_message_t* rxMessage);
    bool sendMessage(uint32_t id, std::array<uint8_t, 8> const& data);
    void sendRequest();

    uint32_t _nextRequestMillis = 0; // When to send next data request to PSU

    std::mutex _mutex;

    TaskHandle_t _taskHandle = nullptr;
    bool _taskDone = false;
    bool _stopLoop = false;

    std::map<HardwareInterface::Property, HardwareInterface::property_t> _stats;
    std::queue<std::pair<HardwareInterface::Setting, uint16_t>> _sendQueue;

    static unsigned constexpr _maxCurrentMultiplier = 20;

    uint8_t _errorCode;
    bool _completeUpdateReceived;
};

} // namespace GridCharger::Huawei
