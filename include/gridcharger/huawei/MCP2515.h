// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>
#include <mutex>
#include <queue>
#include <SPI.h>
#include <mcp_can.h>
#include <SpiManager.h>
#include <gridcharger/huawei/HardwareInterface.h>

namespace GridChargers::Huawei {

class MCP2515 : public HardwareInterface {
public:
    ~MCP2515();

    bool init() final;

    bool getMessage(HardwareInterface::can_message_t& msg) final;

    bool sendMessage(uint32_t canId, std::array<uint8_t, 8> const& data) final;

private:
    // this is static because we cannot give back the bus once we claimed it.
    // as we are going to use a shared host/bus in the future, we won't use a
    // workaround for the limited time we use it like this.
    static std::optional<uint8_t> _oSpiBus;

    std::unique_ptr<SPIClass> _upSPI;
    std::unique_ptr<MCP_CAN> _upCAN;
    gpio_num_t _huaweiIrq; // IRQ pin

    std::atomic<bool> _queueingTaskDone = false;
    std::atomic<bool> _stopQueueing = false;

    static void queueMessages(void* context);
    std::mutex _rxQueueMutex;
    std::queue<HardwareInterface::can_message_t> _rxQueue;
};

} // namespace GridChargers::Huawei
