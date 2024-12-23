// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <atomic>
#include <FreeRTOS.h>
#include <freertos/task.h>
#include <gridcharger/huawei/HardwareInterface.h>

namespace GridCharger::Huawei {

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
};

} // namespace GridCharger::Huawei
