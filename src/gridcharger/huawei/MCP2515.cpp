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

MCP2515::~MCP2515()
{
    std::unique_lock<std::mutex> lock(_mutex);
    _taskDone = false;
    _stopLoop = true;
    lock.unlock();

    if (_taskHandle != nullptr) {
        while (!_taskDone) { delay(10); }
        _taskHandle = nullptr;
    }
}

void MCP2515::staticLoopHelper(void* context)
{
    auto pInstance = static_cast<MCP2515*>(context);
    pInstance->loopHelper();
    vTaskDelete(nullptr);
}

void MCP2515::loopHelper()
{
    std::unique_lock<std::mutex> lock(_mutex);

    while (!_stopLoop) {
        loop();
        lock.unlock();
        yield();
        lock.lock();
    }

    _taskDone = true;
}

bool MCP2515::init() {

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

    uint32_t constexpr stackSize = 2048;
    xTaskCreate(MCP2515::staticLoopHelper, "Huawei:MCP2515",
            stackSize, this, 1/*prio*/, &_taskHandle);

    return true;
}

void MCP2515::loop()
{
    INT32U rxId;
    unsigned char len = 0;
    unsigned char rxBuf[8];

    while (!digitalRead(_huaweiIrq)) {
        _CAN->readMsgBuf(&rxId, &len, rxBuf); // Read data: len = data length, buf = data byte(s)

        // Determine if ID is standard (11 bits) or extended (29 bits)
        if ((rxId & 0x80000000) != 0x80000000) { continue; }

        if (len != 8) { continue; }

        // Other emitted codes not handled here are:
        //     0x1081407E (Ack), 0x1081807E (Ack Frame),
        //     0x1081D27F (Description), 0x1001117E (Whr meter),
        //     0x100011FE (unclear), 0x108111FE (output enabled),
        //     0x108081FE (unclear).
        // https://github.com/craigpeacock/Huawei_R4850G2_CAN/blob/main/r4850.c
        // https://www.beyondlogic.org/review-huawei-r4850g2-power-supply-53-5vdc-3kw/
        if ((rxId & 0x1FFFFFFF) != 0x1081407F) { continue; }

        uint32_t valId = rxBuf[0] << 24 | rxBuf[1] << 16 | rxBuf[2] << 8 | rxBuf[3];
        if ((valId & 0xFF00FFFF) != 0x01000000) { continue; }

        auto property = static_cast<Property>(rxBuf[1]);
        float value = rxBuf[4] << 24 | rxBuf[5] << 16 | rxBuf[6] << 8 | rxBuf[7];

        if (property == HardwareInterface::Property::OutputCurrentMax) {
            value /= _maxCurrentMultiplier;
        }
        else {
            value /= 1024;
        }
        _stats[property] = {value, millis()};
    }

    // Transmit values
    size_t queueSize = _sendQueue.size();
    for (size_t i = 0; i < queueSize; ++i) {
        auto [setting, val] = _sendQueue.front();
        _sendQueue.pop();

        uint8_t data[8] = {
            0x01, static_cast<uint8_t>(setting), 0x00, 0x00,
            0x00, 0x00, static_cast<uint8_t>((val & 0xFF00) >> 8),
            static_cast<uint8_t>(val & 0xFF)
        };

        // Send extended message
        byte sndStat = _CAN->sendMsgBuf(0x108180FE, 1, 8, data);
        if (sndStat != CAN_OK) {
            _errorCode |= HUAWEI_ERROR_CODE_TX;
            _sendQueue.push({setting, val});
        }
    }

    if (_nextRequestMillis < millis()) {
        sendRequest();
        _nextRequestMillis = millis() + HUAWEI_DATA_REQUEST_INTERVAL_MS;
    }

}

HardwareInterface::property_t MCP2515::getParameter(HardwareInterface::Property property)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto pos = _stats.find(property);
    if (pos == _stats.end()) { return {0, 0}; }
    return pos->second;
}

uint8_t MCP2515::getErrorCode(bool clear)
{
    std::lock_guard<std::mutex> lock(_mutex);
    uint8_t e = 0;
    e = _errorCode;
    if (clear) {
        _errorCode = 0;
    }
    return e;
}

void MCP2515::setParameter(HardwareInterface::Setting setting, float val)
{
    std::lock_guard<std::mutex> lock(_mutex);

    switch (setting) {
        case Setting::OfflineVoltage:
        case Setting::OnlineVoltage:
            val *= 1024;
            break;
        case Setting::OfflineCurrent:
        case Setting::OnlineCurrent:
            val *= _maxCurrentMultiplier;
            break;
    }

    _sendQueue.push({setting, static_cast<uint16_t>(val)});
}

// Private methods
// Requests current values from Huawei unit. Response is handled in onReceive
void MCP2515::sendRequest()
{
    uint8_t data[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    //Send extended message
    byte sndStat = _CAN->sendMsgBuf(0x108040FE, 1, 8, data);
    if (sndStat != CAN_OK) {
        _errorCode |= HUAWEI_ERROR_CODE_RX;
    }
}

} // namespace GridCharger::Huawei
