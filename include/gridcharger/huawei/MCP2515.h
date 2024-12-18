// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <SPI.h>
#include <mcp_can.h>
#include <gridcharger/huawei/HardwareInterface.h>

namespace GridCharger::Huawei {

class MCP2515 : public HardwareInterface {
public:
    bool init() final;

    bool getMessage(HardwareInterface::can_message_t& msg) final;

    bool sendMessage(uint32_t valueId, std::array<uint8_t, 8> const& data) final;

private:
    SPIClass *SPI;
    MCP_CAN *_CAN;
    uint8_t _huaweiIrq; // IRQ pin
};

} // namespace GridCharger::Huawei
