// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Malte Schmidt and others
 */
#include <gridcharger/huawei/MCP2515.h>
#include "MessageOutput.h"
#include "SpiManager.h"

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

bool MCP2515::init(uint8_t huawei_miso, uint8_t huawei_mosi, uint8_t huawei_clk,
        uint8_t huawei_irq, uint8_t huawei_cs, uint32_t frequency) {

    auto spi_bus = SpiManagerInst.claim_bus_arduino();
    if (!spi_bus) { return false; }

    SPI = new SPIClass(*spi_bus);

    SPI->begin(huawei_clk, huawei_miso, huawei_mosi, huawei_cs);
    pinMode(huawei_cs, OUTPUT);
    digitalWrite(huawei_cs, HIGH);

    pinMode(huawei_irq, INPUT_PULLUP);
    _huaweiIrq = huawei_irq;

    auto mcp_frequency = MCP_8MHZ;
    if (16000000UL == frequency) { mcp_frequency = MCP_16MHZ; }
    else if (8000000UL != frequency) {
        MessageOutput.printf("Huawei CAN: unknown frequency %d Hz, using 8 MHz\r\n", mcp_frequency);
    }

    _CAN = new MCP_CAN(SPI, huawei_cs);
    if (!_CAN->begin(MCP_STDEXT, CAN_125KBPS, mcp_frequency) == CAN_OK) {
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
  uint8_t i;

  if (!digitalRead(_huaweiIrq)) {
    // If CAN_INT pin is low, read receive buffer
    _CAN->readMsgBuf(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)
    if((rxId & 0x80000000) == 0x80000000) {   // Determine if ID is standard (11 bits) or extended (29 bits)
      if ((rxId & 0x1FFFFFFF) == 0x1081407F && len == 8) {

        int32_t value = __bswap32(*reinterpret_cast<int32_t*>(rxBuf + 4));

        // Input power 0x70, Input frequency 0x71, Input current 0x72
        // Output power 0x73, Efficiency 0x74, Output Voltage 0x75 and Output Current 0x76
        if(rxBuf[1] >= 0x70 && rxBuf[1] <= 0x76 ) {
          _recValues[rxBuf[1] - 0x70] = value;
        }

        // Input voltage
        if(rxBuf[1] == 0x78 ) {
          _recValues[HUAWEI_INPUT_VOLTAGE_IDX] = value;
        }

        // Output Temperature
        if(rxBuf[1] == 0x7F ) {
          _recValues[HUAWEI_OUTPUT_TEMPERATURE_IDX] = value;
        }

        // Input Temperature 0x80, Output Current 1 0x81 and Output Current 2 0x82
        if(rxBuf[1] >= 0x80 && rxBuf[1] <= 0x82 ) {
          _recValues[rxBuf[1] - 0x80 + HUAWEI_INPUT_TEMPERATURE_IDX] = value;
        }

        // This is the last value that is send
        if(rxBuf[1] == 0x81) {
          _completeUpdateReceived = true;
        }
      }
    }
    // Other emitted codes not handled here are: 0x1081407E (Ack), 0x1081807E (Ack Frame), 0x1081D27F (Description), 0x1001117E (Whr meter), 0x100011FE (unclear), 0x108111FE (output enabled), 0x108081FE (unclear). See:
    // https://github.com/craigpeacock/Huawei_R4850G2_CAN/blob/main/r4850.c
    // https://www.beyondlogic.org/review-huawei-r4850g2-power-supply-53-5vdc-3kw/
  }

  // Transmit values
  for (i = 0; i < HUAWEI_OFFLINE_CURRENT; i++) {
    if ( _hasNewTxValue[i] == true) {
      uint8_t data[8] = {0x01, i, 0x00, 0x00, 0x00, 0x00, (uint8_t)((_txValues[i] & 0xFF00) >> 8), (uint8_t)(_txValues[i] & 0xFF)};

      // Send extended message
      byte sndStat = _CAN->sendMsgBuf(0x108180FE, 1, 8, data);
      if (sndStat == CAN_OK) {
        _hasNewTxValue[i] = false;
      } else {
        _errorCode |= HUAWEI_ERROR_CODE_TX;
      }
    }
  }

  if (_nextRequestMillis < millis()) {
    sendRequest();
    _nextRequestMillis = millis() + HUAWEI_DATA_REQUEST_INTERVAL_MS;
  }

}

int32_t MCP2515::getParameterValue(uint8_t parameter)
{
  std::lock_guard<std::mutex> lock(_mutex);
  if (parameter < HUAWEI_OUTPUT_CURRENT1_IDX) {
    return _recValues[parameter];
  }
  return 0;
}

bool MCP2515::gotNewRxDataFrame(bool clear)
{
  std::lock_guard<std::mutex> lock(_mutex);
  bool b = false;
  b = _completeUpdateReceived;
  if (clear) {
    _completeUpdateReceived = false;
  }
  return b;
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

void MCP2515::setParameterValue(uint16_t in, uint8_t parameterType)
{
  std::lock_guard<std::mutex> lock(_mutex);
  if (parameterType < HUAWEI_OFFLINE_CURRENT) {
    _txValues[parameterType] = in;
    _hasNewTxValue[parameterType] = true;
  }
}

// Private methods
// Requests current values from Huawei unit. Response is handled in onReceive
void MCP2515::sendRequest()
{
    uint8_t data[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    //Send extended message
    byte sndStat = _CAN->sendMsgBuf(0x108040FE, 1, 8, data);
    if(sndStat != CAN_OK) {
        _errorCode |= HUAWEI_ERROR_CODE_RX;
    }
}

} // namespace GridCharger::Huawei
