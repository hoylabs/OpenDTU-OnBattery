// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <atomic>
#include <array>
#include <mutex>
#include <map>
#include <queue>
#include <cstdint>
#include <FreeRTOS.h>
#include <freertos/task.h>

namespace GridCharger::Huawei {

class HardwareInterface {
public:
    HardwareInterface() = default;

    virtual ~HardwareInterface() = default;

    virtual bool init() = 0;

    enum class Setting : uint8_t {
        OnlineVoltage = 0,
        OfflineVoltage = 1,
        OnlineCurrent = 3,
        OfflineCurrent = 4
    };
    void setParameter(Setting setting, float val);

    enum class Property : uint8_t {
        InputPower = 0x70,
        InputFrequency = 0x71,
        InputCurrent = 0x72,
        OutputPower = 0x73,
        Efficiency = 0x74,
        OutputVoltage = 0x75,
        OutputCurrentMax = 0x76,
        InputVoltage = 0x78,
        OutputTemperature = 0x7F,
        InputTemperature = 0x80,
        OutputCurrent = 0x81
    };
    using property_t = std::pair<float, uint32_t>; // value and timestamp
    property_t getParameter(Property prop) const;

    static uint32_t constexpr DataRequestIntervalMillis = 2500;

protected:
    struct CAN_MESSAGE_T {
        uint32_t canId;
        uint32_t valueId;
        int32_t value;
    };
    using can_message_t = struct CAN_MESSAGE_T;

    bool startLoop();
    void stopLoop();

    TaskHandle_t getTaskHandle() const { return _taskHandle; }

private:
    static void staticLoopHelper(void* context);
    void loop();

    virtual bool getMessage(can_message_t& msg) = 0;

    virtual bool sendMessage(uint32_t canId, std::array<uint8_t, 8> const& data) = 0;

    mutable std::mutex _mutex;

    TaskHandle_t _taskHandle = nullptr;
    std::atomic<bool> _taskDone = false;
    bool _stopLoop = false;

    std::map<HardwareInterface::Property, HardwareInterface::property_t> _stats;

    std::queue<std::pair<HardwareInterface::Setting, uint16_t>> _sendQueue;

    static unsigned constexpr _maxCurrentMultiplier = 20;

    uint32_t _nextRequestMillis = 0; // When to send next data request to PSU
};

} // namespace GridCharger::Huawei
