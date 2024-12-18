// SPDX-License-Identifier: GPL-2.0-or-later

#include <Arduino.h>
#include <MessageOutput.h>
#include <gridcharger/huawei/HardwareInterface.h>

namespace GridCharger::Huawei {

void HardwareInterface::staticLoopHelper(void* context)
{
    auto pInstance = static_cast<HardwareInterface*>(context);
    static auto constexpr resetNotificationValue = pdTRUE;
    static auto constexpr notificationTimeout = pdMS_TO_TICKS(500);

    while (true) {
        ulTaskNotifyTake(resetNotificationValue, notificationTimeout);
        {
            std::unique_lock<std::mutex> lock(pInstance->_mutex);
            if (pInstance->_stopLoop) { break; }
            pInstance->loop();
        }
    }

    pInstance->_taskDone = true;

    vTaskDelete(nullptr);
}

bool HardwareInterface::startLoop()
{
    uint32_t constexpr stackSize = 4096;
    return pdPASS == xTaskCreate(HardwareInterface::staticLoopHelper,
            "HuaweiHwIfc", stackSize, this, 1/*prio*/, &_taskHandle);
}

void HardwareInterface::stopLoop()
{
    if (_taskHandle == nullptr) { return; }

    _taskDone = false;

    {
        std::unique_lock<std::mutex> lock(_mutex);
        _stopLoop = true;
    }

    xTaskNotifyGive(_taskHandle);

    while (!_taskDone) { delay(10); }
    _taskHandle = nullptr;
}

void HardwareInterface::loop()
{
    can_message_t msg;

    while (getMessage(msg)) {
        // Other emitted codes not handled here are:
        //     0x1081407E (Ack), 0x1081807E (Ack Frame),
        //     0x1081D27F (Description), 0x1001117E (Whr meter),
        //     0x100011FE (unclear), 0x108111FE (output enabled),
        //     0x108081FE (unclear).
        // https://github.com/craigpeacock/Huawei_R4850G2_CAN/blob/main/r4850.c
        // https://www.beyondlogic.org/review-huawei-r4850g2-power-supply-53-5vdc-3kw/
        if ((msg.canId & 0x1FFFFFFF) != 0x1081407F) { continue; }

        if ((msg.valueId & 0xFF00FFFF) != 0x01000000) { continue; }

        auto property = static_cast<Property>((msg.valueId & 0x00FF0000) >> 16);

        unsigned divisor = (property == Property::OutputCurrentMax) ? _maxCurrentMultiplier : 1024;
        _stats[property] = {static_cast<float>(msg.value)/divisor, millis()};
    }

    size_t queueSize = _sendQueue.size();
    for (size_t i = 0; i < queueSize; ++i) {
        auto [setting, val] = _sendQueue.front();
        _sendQueue.pop();

        std::array<uint8_t, 8> data = {
            0x01, static_cast<uint8_t>(setting), 0x00, 0x00,
            0x00, 0x00, static_cast<uint8_t>((val & 0xFF00) >> 8),
            static_cast<uint8_t>(val & 0xFF)
        };

        if (!sendMessage(0x108180FE, data)) {
            MessageOutput.print("[Huawei::HwIfc] Failed to set parameter\r\n");
            _sendQueue.push({setting, val});
        }
    }

    if (_nextRequestMillis < millis()) {
        static constexpr std::array<uint8_t, 8> data = { 0 };
        if (!sendMessage(0x108040FE, data)) {
            MessageOutput.print("[Huawei::HwIfc] Failed to send data request\r\n");
        }

        _nextRequestMillis = millis() + DataRequestIntervalMillis;
    }
}

void HardwareInterface::setParameter(HardwareInterface::Setting setting, float val)
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_taskHandle == nullptr) { return; }

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

    xTaskNotifyGive(_taskHandle);
}

HardwareInterface::property_t HardwareInterface::getParameter(HardwareInterface::Property property) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto pos = _stats.find(property);
    if (pos == _stats.end()) { return {0, 0}; }
    return pos->second;
}

} // namespace GridCharger::Huawei
