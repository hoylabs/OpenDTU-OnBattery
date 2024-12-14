// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <FreeRTOS.h>
#include <freertos/task.h>
#include <cstdint>
#include "SPI.h"
#include <mcp_can.h>
#include <mutex>

namespace GridCharger::Huawei {

// Index values for rec_values array
#define HUAWEI_INPUT_POWER_IDX 0
#define HUAWEI_INPUT_FREQ_IDX 1
#define HUAWEI_INPUT_CURRENT_IDX 2
#define HUAWEI_OUTPUT_POWER_IDX 3
#define HUAWEI_EFFICIENCY_IDX 4
#define HUAWEI_OUTPUT_VOLTAGE_IDX 5
#define HUAWEI_OUTPUT_CURRENT_MAX_IDX 6
#define HUAWEI_INPUT_VOLTAGE_IDX 7
#define HUAWEI_OUTPUT_TEMPERATURE_IDX 8
#define HUAWEI_INPUT_TEMPERATURE_IDX 9
#define HUAWEI_OUTPUT_CURRENT_IDX 10
#define HUAWEI_OUTPUT_CURRENT1_IDX 11

// Defines and index values for tx_values array
#define HUAWEI_OFFLINE_VOLTAGE 0x01
#define HUAWEI_ONLINE_VOLTAGE 0x00
#define HUAWEI_OFFLINE_CURRENT 0x04
#define HUAWEI_ONLINE_CURRENT 0x03

// Error codes
#define HUAWEI_ERROR_CODE_RX 0x01
#define HUAWEI_ERROR_CODE_TX 0x02

// Updateinterval used to request new values from the PSU
#define HUAWEI_DATA_REQUEST_INTERVAL_MS 2500

class MCP2515 {
public:
    ~MCP2515();

    bool init(uint8_t huawei_miso, uint8_t huawei_mosi, uint8_t huawei_clk,
            uint8_t huawei_irq, uint8_t huawei_cs, uint32_t frequency);
    bool gotNewRxDataFrame(bool clear);
    uint8_t getErrorCode(bool clear);
    int32_t getParameterValue(uint8_t parameter);
    void setParameterValue(uint16_t in, uint8_t parameterType);

private:
    static void staticLoopHelper(void* context);
    void loopHelper();
    void loop();
    void sendRequest();

    SPIClass *SPI;
    MCP_CAN *_CAN;
    uint8_t _huaweiIrq; // IRQ pin
    uint32_t _nextRequestMillis = 0; // When to send next data request to PSU

    std::mutex _mutex;

    TaskHandle_t _taskHandle = nullptr;
    bool _taskDone = false;
    bool _stopLoop = false;

    int32_t _recValues[12];
    uint16_t _txValues[5];
    bool _hasNewTxValue[5];

    uint8_t _errorCode;
    bool _completeUpdateReceived;
};

} // namespace GridCharger::Huawei
