#pragma once

#include <memory>
#include <vector>
#include <frozen/string.h>

#include <battery/Provider.h>
#include <battery/jkbms/Stats.h>
#include <battery/jkbms/DataPoints.h>
#include <battery/jkbms/SerialMessage.h>
#include <battery/jkbms/Dummy.h>
#include <battery/jkbms/HassIntegration.h>

//#define JKBMS_DUMMY_SERIAL

namespace Batteries::JkBms {

class Provider : public ::Batteries::Provider {
public:
    Provider();

    bool init() final;
    void deinit() final;
    void loop() final;
    std::shared_ptr<::Batteries::Stats> getStats() const final { return _stats; }
    std::shared_ptr<::Batteries::HassIntegration> getHassIntegration() final { return _hassIntegration; }

private:
    static char constexpr _serialPortOwner[] = "JK BMS";

#ifdef JKBMS_DUMMY_SERIAL
    std::unique_ptr<DummySerial> _upSerial;
#else
    std::unique_ptr<HardwareSerial> _upSerial;
#endif

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
    void sendRequest(uint8_t pollInterval);
    void rxData(uint8_t inbyte);
    void reset();
    void frameComplete();
    void processDataPoints(DataPointContainer const& dataPoints);

    enum class Interface : unsigned {
        Invalid,
        Uart,
        Transceiver
    };

    Interface getInterface() const;

    enum class ReadState : unsigned {
        Idle,
        WaitingForFrameStart,
        FrameStartReceived,
        StartMarkerReceived,
        FrameLengthMsbReceived,
        ReadingFrame
    };
    ReadState _readState;
    void setReadState(ReadState state) {
        _readState = state;
    }

    gpio_num_t _rxEnablePin = GPIO_NUM_NC;
    gpio_num_t _txEnablePin = GPIO_NUM_NC;
    Status _lastStatus = Status::Initializing;
    uint32_t _lastStatusPrinted = 0;
    uint32_t _lastRequest = 0;
    uint16_t _frameLength = 0;
    uint8_t _protocolVersion = -1;
    SerialResponse::tData _buffer = {};
    std::shared_ptr<Stats> _stats;
    std::shared_ptr<HassIntegration> _hassIntegration;
};

} // namespace Batteries::JkBms
