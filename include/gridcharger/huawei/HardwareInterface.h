// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <atomic>
#include <array>
#include <mutex>
#include <memory>
#include <queue>
#include <string>
#include <tuple>
#include <cstdint>
#include <gridcharger/huawei/DataPoints.h>

namespace GridCharger::Huawei {

class HardwareInterface {
public:
    HardwareInterface() = default;

    virtual ~HardwareInterface() = default;

    virtual bool init() = 0;

    enum class Setting : uint8_t {
        OnlineVoltage = 0x00,
        OfflineVoltage = 0x01,
        OnlineCurrent = 0x03,
        OfflineCurrent = 0x04
    };
    void setParameter(Setting setting, float val);

    void setProduction(bool enable);

    std::unique_ptr<DataPointContainer> getCurrentData();

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

    std::unique_ptr<DataPointContainer> _upData = nullptr;

    struct COMMAND {
        uint8_t command;
        uint16_t flags;
        uint32_t value;
    };
    using command_t = struct COMMAND;
    std::queue<command_t> _sendQueue;

    static unsigned constexpr _maxCurrentMultiplier = 20;

    uint32_t _nextRequestMillis = 0; // When to send next data request to PSU

    bool readBoardProperties(can_message_t const& msg);

    std::string _boardProperties = "";
    uint16_t _boardPropertiesCounter = 0;
    enum class StringState : uint8_t {
        Unknown,
        RequestFailed,
        Reading,
        MissedMessage,
        Complete
    };
    StringState _boardPropertiesState = StringState::Unknown;
    uint32_t _boardPropertiesRequestMillis = 0;

    bool readRectifierState(can_message_t const& msg);
};

} // namespace GridCharger::Huawei
