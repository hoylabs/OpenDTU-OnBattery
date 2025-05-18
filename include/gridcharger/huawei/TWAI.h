// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <atomic>
#include <mutex>
#include <queue>
#include <gridcharger/huawei/HardwareInterface.h>

namespace GridChargers::Huawei {

class TWAI : public HardwareInterface {
public:
    ~TWAI();

    bool init() final;

    bool getMessage(HardwareInterface::can_message_t& msg) final;

    bool sendMessage(uint32_t canId, std::array<uint8_t, 8> const& data) final;

private:
    TaskHandle_t _pollingTaskHandle = nullptr;
    std::atomic<bool> _pollingTaskDone = false;
    std::atomic<bool> _stopPolling = false;

    static void pollAlerts(void* context);
    std::mutex _rxQueueMutex;
    std::queue<HardwareInterface::can_message_t> _rxQueue;
};

} // namespace GridChargers::Huawei
