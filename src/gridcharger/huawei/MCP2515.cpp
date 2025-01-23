// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Malte Schmidt and others
 */
#include <gridcharger/huawei/MCP2515.h>
#include "MessageOutput.h"
#include "PinMapping.h"
#include "Configuration.h"

namespace GridCharger::Huawei {

TaskHandle_t sIsrTaskHandle = nullptr;

void mcp2515Isr()
{
    if (sIsrTaskHandle == nullptr) { return; }
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(sIsrTaskHandle, &xHigherPriorityTaskWoken);
    // make sure that the high-priority hardware interface task is scheduled,
    // as the timing is very critical. CAN messages will be missed if the
    // MCP2515 interrupt is not serviced immediately, as a new message
    // overwrites a pending message.
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

std::optional<uint8_t> MCP2515::_oSpiBus = std::nullopt;

MCP2515::~MCP2515()
{
    detachInterrupt(digitalPinToInterrupt(_huaweiIrq));
    sIsrTaskHandle = nullptr;
    stopLoop();
    _upCAN.reset(nullptr);
    if (_upSPI) { _upSPI->end(); } // nullptr if init failed or never called
    _upSPI.reset(nullptr);
}

bool MCP2515::init()
{
    const PinMapping_t& pin = PinMapping.get();

    MessageOutput.printf("[Huawei::MCP2515] clk = %d, miso = %d, mosi = %d, cs = %d, irq = %d\r\n",
            pin.huawei_clk, pin.huawei_miso, pin.huawei_mosi, pin.huawei_cs, pin.huawei_irq);

    if (pin.huawei_clk < 0 || pin.huawei_miso < 0 || pin.huawei_mosi < 0 || pin.huawei_cs < 0 || pin.huawei_irq < 0) {
        MessageOutput.printf("[Huawei::MCP2515] invalid pin config\r\n");
        return false;
    }

    if (!_oSpiBus) {
        _oSpiBus = SpiManagerInst.claim_bus_arduino();
    }

    if (!_oSpiBus) {
        MessageOutput.printf("[Huawei::MCP2515] no SPI host available\r\n");
        return false;
    }

    _upSPI = std::make_unique<SPIClass>(*_oSpiBus);

    _upSPI->begin(pin.huawei_clk, pin.huawei_miso, pin.huawei_mosi, pin.huawei_cs);
    pinMode(pin.huawei_cs, OUTPUT);
    digitalWrite(pin.huawei_cs, HIGH);

    auto mcp_frequency = MCP_8MHZ;
    auto frequency = Configuration.get().Huawei.CAN_Controller_Frequency;
    if (16000000UL == frequency) { mcp_frequency = MCP_16MHZ; }
    else if (8000000UL != frequency) {
        MessageOutput.printf("[Huawei::MCP2515] unknown frequency %d Hz, using 8 MHz\r\n", mcp_frequency);
    }

    _upCAN = std::make_unique<MCP_CAN>(_upSPI.get(), pin.huawei_cs);
    if (_upCAN->begin(MCP_STDEXT, CAN_125KBPS, mcp_frequency) != CAN_OK) {
        MessageOutput.printf("[Huawei::MCP2515] mcp_can begin() failed\r\n");
        return false;
    }

    const uint32_t myMask = 0xFFFFFFFF;         // Look at all incoming bits and...
    const uint32_t myFilter = 0x1081407F;       // filter for this message only
    _upCAN->init_Mask(0, 1, myMask);
    _upCAN->init_Filt(0, 1, myFilter);
    _upCAN->init_Mask(1, 1, myMask);

    // Change to normal mode to allow messages to be transmitted
    _upCAN->setMode(MCP_NORMAL);

    if (!startLoop()) {
        MessageOutput.printf("[Huawei::MCP2515] failed to start loop task\r\n");
        return false;
    }

    if (sIsrTaskHandle != nullptr) {
        // make the ISR aware of multiple instances if multiple instances of
        // this driver should be able to co-exist. only one is supported now.
        MessageOutput.printf("[Huawei::MCP2515] ISR task handle already in use\r\n");
        stopLoop();
        return false;
    }

    sIsrTaskHandle = getTaskHandle();
    _huaweiIrq = pin.huawei_irq;
    pinMode(_huaweiIrq, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(_huaweiIrq), mcp2515Isr, FALLING);

    return true;
}

bool MCP2515::getMessage(HardwareInterface::can_message_t& msg)
{
    if (!_upCAN) { return false; }

    INT32U rxId;
    unsigned char len = 0;
    unsigned char rxBuf[8];

    while (!digitalRead(_huaweiIrq)) {
        _upCAN->readMsgBuf(&rxId, &len, rxBuf); // Read data: len = data length, buf = data byte(s)

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

bool MCP2515::sendMessage(uint32_t canId, std::array<uint8_t, 8> const& data)
{
    if (!_upCAN) { return false; }

    // MCP2515 CAN library requires a non-const pointer to the data
    uint8_t rwData[8];
    memcpy(rwData, data.data(), data.size());
    return _upCAN->sendMsgBuf(canId, 1, 8, rwData) == CAN_OK;
}

} // namespace GridCharger::Huawei
