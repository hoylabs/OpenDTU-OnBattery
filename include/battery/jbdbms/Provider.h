#pragma once

#include <memory>
#include <vector>
#include <frozen/string.h>

#include <battery/Provider.h>
#include <battery/jbdbms/Stats.h>
#include <battery/jbdbms/DataPoints.h>
#include <battery/jbdbms/SerialMessage.h>
#include <battery/jbdbms/HassIntegration.h>

namespace Batteries::JbdBms {

class Provider : public ::Batteries::Provider {
public:
    Provider();

    bool init(bool verboseLogging) final;
    void deinit() final;
    void loop() final;
    std::shared_ptr<::Batteries::Stats> getStats() const final { return _stats; }
    std::shared_ptr<::Batteries::HassIntegration> getHassIntegration() final { return _hassIntegration; }

private:
    static char constexpr _serialPortOwner[] = "JBD BMS";

#ifdef JBDBMS_DUMMY_SERIAL
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
        FrameStartReceived, // 1 Byte: 0xDD
        StateReceived,
        CommandCodeReceived,
        ReadingDataContent,
        DataContentReceived,
        ReadingCheckSum,
        CheckSumReceived,
    };

    ReadState _readState;
    void setReadState(ReadState state) {
        _readState = state;
    }

    bool _verboseLogging = true;
    int8_t _rxEnablePin = -1;
    int8_t _txEnablePin = -1;
    Status _lastStatus = Status::Initializing;
    uint32_t _lastStatusPrinted = 0;
    uint32_t _lastRequest = 0;
    uint8_t _dataLength = 0;
    JbdBms::SerialResponse::tData _buffer = {};
    std::shared_ptr<Stats> _stats;
    std::shared_ptr<HassIntegration> _hassIntegration;
};

} // namespace Batteries::JbdBms
