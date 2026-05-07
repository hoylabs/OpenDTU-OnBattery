// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>
#include <vector>
#include <frozen/string.h>

#include <battery/Provider.h>
#include <battery/pytes/rs485/DataPoints.h>
#include <battery/pytes/rs485/Parsers.h>
#include <battery/pytes/rs485/Stats.h>
#include <battery/pytes/rs485/HassIntegration.h>
#include <battery/pytes/rs485/SerialCommand.h>
#include <battery/pytes/rs485/SerialResponse.h>

namespace Batteries::Pytes::Rs485 {

class Provider : public ::Batteries::Provider {
public:
    Provider();

    bool init() final;
    void deinit() final;
    void loop() final;

    std::shared_ptr<::Batteries::Stats> getStats() const final { return _stats; }
    std::shared_ptr<::Batteries::HassIntegration> getHassIntegration() final { return _hassIntegration; }

private:
    static char constexpr _serialPortOwner[] = "Pytes RS485";

    std::unique_ptr<HardwareSerial> _upSerial;

    enum class Status : unsigned {
        Initializing,
        Timeout,
        WaitingForPollInterval,
        HwSerialNotAvailableForWrite,
        BusyReading,
        RequestSent,
        FrameCompleted
    };

    frozen::string const& getStatusText(Status status);
    void announceStatus(Status status);

    // Query step: which command are we currently waiting for a response to
    enum class QueryStep : unsigned {
        PackBasic,     // 0x60 – sent only on first poll
        ModuleBasic,   // 0x80 – first poll only, per module
        ModuleProtect, // 0x82 – first poll only, per module (static protection thresholds)
        PackAnalog,    // 0x61 – live data, every poll
        PackChgDsg,    // 0x62 – cluster charge/discharge limits, every poll
        ModuleChgDsg,  // 0x83 – per-module charge/discharge limits, every poll
        ModuleAnalog,  // 0x81 – ambient temp, cycles, balance, every poll, per module
        ModuleCells,   // 0x92 – per-cell voltage, current, temperature, SOC, every poll, per module
        Done,          // all responses received for this poll cycle
    };

    void sendRequest(uint8_t pollInterval);
    void sendCommand(SerialCommand::Command cmd, std::vector<uint8_t> info = {});
    void rxData(uint8_t inbyte);
    void reset();
    void frameComplete();

    enum class Interface : unsigned {
        Invalid,
        Uart,
        Transceiver
    };

    Interface getInterface() const;

    enum class ReadState : unsigned {
        Idle,
        WaitingForFrameStart,
        ReadingFrame,
    };

    ReadState _readState = ReadState::Idle;
    void setReadState(ReadState state) { _readState = state; }

    gpio_num_t _rxEnablePin = GPIO_NUM_NC;
    gpio_num_t _txEnablePin = GPIO_NUM_NC;

    Status _lastStatus = Status::Initializing;
    uint32_t _lastStatusPrinted = 0;
    uint32_t _lastRequest = 0;
    uint32_t _lastCycleStart = 0;

    std::vector<uint8_t> _rxBuffer;

    QueryStep _queryStep = QueryStep::PackBasic;
    bool _firstPoll = true;
    uint8_t _numModules = 1;      // populated from 0x61; safe default of 1
    uint8_t _currentModuleNo = 1; // 1-based, cycles 1.._numModules

    std::shared_ptr<Stats> _stats;
    std::shared_ptr<HassIntegration> _hassIntegration;
};

} // namespace Batteries::Pytes::Rs485
