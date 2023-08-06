#pragma once

#include <memory>
#include <vector>

#include "JkBmsSerialMessage.h"

class DataPointContainer;
class JkBmsBatteryStats;

namespace JkBms {

class ControllerClass {
    public:
        ControllerClass() = default;

        void init(int8_t rx, int8_t rxEnableNot, int8_t tx, int8_t txEnable,
                std::shared_ptr<JkBmsBatteryStats> stats);
        void deinit();
        void loop();

        enum class Status : unsigned {
            Initializing,
            InvalidTransceiverConfig,
            DisabledByConfig,
            Timeout,
            WaitingForPollInterval,
            HwSerialNotAvailableForWrite,
            BusyReading,
            RequestSent,
            FrameCompleted
        };

    private:
        std::string const& getStatusText(Status status);
        void announceStatus(Status status);
        void sendRequest(uint8_t pollInterval);
        void rxData(uint8_t inbyte);
        void reset();
        void frameComplete();
        void processDataPoints(DataPointContainer const& dataPoints);

        enum class Interface : unsigned {
            Disabled,
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

        int8_t _rxEnablePin = -1;
        int8_t _txEnablePin = -1;
        Status _lastStatus = Status::Initializing;
        uint32_t _lastStatusPrinted = 0;
        uint32_t _lastRequest = 0;
        uint16_t _frameLength = 0;
        uint8_t _protocolVersion = -1;
        SerialResponse::tData _buffer = {};
        std::shared_ptr<JkBmsBatteryStats> _stats;
};

extern ControllerClass Controller;

} /* namespace JkBms */
