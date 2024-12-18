// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Malte Schmidt and others
 */
#include <gridcharger/huawei/MCP2515.h>
#include "MessageOutput.h"
#include "SpiManager.h"
#include "PinMapping.h"
#include "Configuration.h"

namespace GridCharger::Huawei {

bool MCP2515::init()
{
    const PinMapping_t& pin = PinMapping.get();

    MessageOutput.printf("[Huawei::MCP2515] clk = %d, miso = %d, mosi = %d, cs = %d, irq = %d\r\n",
            pin.huawei_clk, pin.huawei_miso, pin.huawei_mosi, pin.huawei_cs, pin.huawei_irq);

    if (pin.huawei_clk < 0 || pin.huawei_miso < 0 || pin.huawei_mosi < 0 || pin.huawei_cs < 0) {
        MessageOutput.printf("[Huawei::MCP2515] invalid pin config\r\n");
        return false;
    }

    auto spi_bus = SpiManagerInst.claim_bus_arduino();
    if (!spi_bus) { return false; }

    SPI = new SPIClass(*spi_bus);

    SPI->begin(pin.huawei_clk, pin.huawei_miso, pin.huawei_mosi, pin.huawei_cs);
    pinMode(pin.huawei_cs, OUTPUT);
    digitalWrite(pin.huawei_cs, HIGH);

    pinMode(pin.huawei_irq, INPUT_PULLUP);
    _huaweiIrq = pin.huawei_irq;

    auto mcp_frequency = MCP_8MHZ;
    auto frequency = Configuration.get().Huawei.CAN_Controller_Frequency;
    if (16000000UL == frequency) { mcp_frequency = MCP_16MHZ; }
    else if (8000000UL != frequency) {
        MessageOutput.printf("[Huawei::MCP2515] unknown frequency %d Hz, using 8 MHz\r\n", mcp_frequency);
    }

    _CAN = new MCP_CAN(SPI, pin.huawei_cs);
    if (_CAN->begin(MCP_STDEXT, CAN_125KBPS, mcp_frequency) != CAN_OK) {
        MessageOutput.printf("[Huawei::MCP2515] mcp_can begin() failed\r\n");
        return false;
    }

    const uint32_t myMask = 0xFFFFFFFF;         // Look at all incoming bits and...
    const uint32_t myFilter = 0x1081407F;       // filter for this message only
    _CAN->init_Mask(0, 1, myMask);
    _CAN->init_Filt(0, 1, myFilter);
    _CAN->init_Mask(1, 1, myMask);

    // Change to normal mode to allow messages to be transmitted
    _CAN->setMode(MCP_NORMAL);

    return startLoop();
}

bool MCP2515::getMessage(HardwareInterface::can_message_t& msg)
{
    INT32U rxId;
    unsigned char len = 0;
    unsigned char rxBuf[8];

    while (!digitalRead(_huaweiIrq)) {
        _CAN->readMsgBuf(&rxId, &len, rxBuf); // Read data: len = data length, buf = data byte(s)

        // Determine if ID is standard (11 bits) or extended (29 bits)
        if ((rxId & 0x80000000) != 0x80000000) { continue; }

        if (len != 8) { continue; }

        msg.canId = rxId;
        msg.valueId = rxBuf[0] << 24 | rxBuf[1] << 16 | rxBuf[2] << 8 | rxBuf[3];
        msg.value = rxBuf[4] << 24 | rxBuf[5] << 16 | rxBuf[6] << 8 | rxBuf[7];

        return true;
    }

    return false;
}

bool MCP2515::sendMessage(uint32_t valueId, std::array<uint8_t, 8> const& data)
{
    // MCP2515 CAN library requires a non-const pointer to the data
    uint8_t rwData[8];
    memcpy(rwData, data.data(), data.size());
    return _CAN->sendMsgBuf(valueId, 1, 8, rwData) == CAN_OK;
}

} // namespace GridCharger::Huawei
