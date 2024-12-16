// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Malte Schmidt and others
 */
#include <gridcharger/huawei/TWAI.h>
#include "MessageOutput.h"
#include "PinMapping.h"
#include "Configuration.h"
#include <driver/twai.h>

namespace GridCharger::Huawei {

TWAI::~TWAI()
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

void TWAI::staticLoopHelper(void* context)
{
    auto pInstance = static_cast<TWAI*>(context);
    pInstance->loopHelper();
    vTaskDelete(nullptr);
}

void TWAI::loopHelper()
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

bool TWAI::init() {

    const PinMapping_t& pin = PinMapping.get();

    MessageOutput.printf("[Huawei::TWAI] rx = %d, tx = %d\r\n",
            pin.huawei_rx, pin.huawei_tx);

    if (pin.huawei_rx < 0 || pin.huawei_tx < 0) {
        MessageOutput.print("[Huawei::TWAI] invalid pin config\r\n");
        return false;
    }

    auto tx = static_cast<gpio_num_t>(pin.huawei_tx);
    auto rx = static_cast<gpio_num_t>(pin.huawei_rx);
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(tx, rx, TWAI_MODE_NORMAL);

    // interrupts at level 1 are in high demand, at least on ESP32-S3 boards,
    // but only a limited amount can be allocated. failing to allocate an
    // interrupt in the TWAI driver will cause a bootloop. we therefore
    // register the TWAI driver's interrupt at level 2. level 2 interrupts
    // should be available -- we don't really know. we would love to have the
    // esp_intr_dump() function, but that's not available yet in our version
    // of the underlying esp-idf.
    g_config.intr_flags = ESP_INTR_FLAG_LEVEL2;

    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        MessageOutput.print("[Huawei::TWAI] Failed to install driver\r\n");
        return false;
    }

    if (twai_start() != ESP_OK) {
        MessageOutput.print("[Huawei::TWAI] Failed to start driver\r\n");
        return false;
    }

    MessageOutput.print("[Huawei::TWAI] driver ready\r\n");

    uint32_t constexpr stackSize = 3072;
    xTaskCreate(TWAI::staticLoopHelper, "Huawei:TWAI",
            stackSize, this, 1/*prio*/, &_taskHandle);

    return true;
}

bool TWAI::receiveMessage(twai_message_t* rxMessage)
{
    twai_status_info_t status;
    if (twai_get_status_info(&status) != ESP_OK) {
        MessageOutput.print("[Huawei::TWAI] Failed to get status info\r\n");
        return false;
    }

    if (status.msgs_to_rx == 0) { return false; }

    // Wait for message to be received, function is blocking (for 100ms)
    if (twai_receive(rxMessage, pdMS_TO_TICKS(100)) != ESP_OK) {
        MessageOutput.print("[Huawei::TWAI] Failed to receive message\r\n");
        return false;
    }

    return true;
}

bool TWAI::sendMessage(uint32_t id, std::array<uint8_t, 8> const& data)
{
    twai_message_t txMsg;
    memset(&txMsg, 0, sizeof(txMsg));
    memcpy(txMsg.data, data.data(), data.size());
    txMsg.extd = 1;
    txMsg.data_length_code = data.size();
    txMsg.identifier = id;

    return twai_transmit(&txMsg, pdMS_TO_TICKS(1000)) == ESP_OK;
}

void TWAI::loop()
{
    twai_message_t rxMessage;

    while (receiveMessage(&rxMessage)) {
        if (rxMessage.extd != 1) { continue; } // we only process extended format messages

        if (rxMessage.data_length_code != 8) { continue; }

        if ((rxMessage.identifier & 0x1FFFFFFF) != 0x1081407F) { continue; }

        uint32_t valId = rxMessage.data[0] << 24 | rxMessage.data[1] << 16 | rxMessage.data[2] << 8 | rxMessage.data[3];
        if ((valId & 0xFF00FFFF) != 0x01000000) { continue; }

        auto property = static_cast<Property>(rxMessage.data[1]);
        float value = rxMessage.data[4] << 24 | rxMessage.data[5] << 16 | rxMessage.data[6] << 8 | rxMessage.data[7];

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

        std::array<uint8_t, 8> data = {
            0x01, static_cast<uint8_t>(setting), 0x00, 0x00,
            0x00, 0x00, static_cast<uint8_t>((val & 0xFF00) >> 8),
            static_cast<uint8_t>(val & 0xFF)
        };

        // Send extended message
        if (!sendMessage(0x108180FE, data)) {
            _errorCode |= HUAWEI_ERROR_CODE_TX;
            _sendQueue.push({setting, val});
        }
    }

    if (_nextRequestMillis < millis()) {
        sendRequest();
        _nextRequestMillis = millis() + HUAWEI_DATA_REQUEST_INTERVAL_MS;
    }
}

HardwareInterface::property_t TWAI::getParameter(HardwareInterface::Property property)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto pos = _stats.find(property);
    if (pos == _stats.end()) { return {0, 0}; }
    return pos->second;
}

uint8_t TWAI::getErrorCode(bool clear)
{
    std::lock_guard<std::mutex> lock(_mutex);
    uint8_t e = 0;
    e = _errorCode;
    if (clear) {
        _errorCode = 0;
    }
    return e;
}

void TWAI::setParameter(HardwareInterface::Setting setting, float val)
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

void TWAI::sendRequest()
{
    std::array<uint8_t, 8> data = { 0 };
    if (!sendMessage(0x108040FE, data)) {
        _errorCode |= HUAWEI_ERROR_CODE_RX;
    }
}

} // namespace GridCharger::Huawei
