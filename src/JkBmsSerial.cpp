//#include <Arduino.h>
#include "HardwareSerial.h"
#include "MessageOutput.h"
#include "JkBmsSerial.h"
#include <map>

HardwareSerial HwSerial(2);

namespace JkBms {

JkBmsSerial Controller;

void JkBmsSerial::init(int8_t rx, int8_t tx)
{
    MessageOutput.printf("JkBmsSerial::init, rx=%d, tx=%d\r\n", rx, tx);
    HwSerial.begin(115200, SERIAL_8N1, rx, tx);
    HwSerial.flush();
}

std::string const& JkBmsSerial::getStatusText(JkBmsSerial::Status status)
{
    static const std::string missing =  "programmer error: missing status text";

    static const std::map<Status, const std::string> texts = {
        { Status::DisabledByConfig, "disabled by configuration" },
        { Status::Timeout, "timeout wating for response from BMS" },
        { Status::WaitingForPollInterval, "waiting for poll interval to elapse" },
        { Status::HwSerialNotAvailableForWrite, "UART is not available for writing" },
        { Status::BusyReading, "busy reading a message from the BMS" },
        { Status::RequestSent, "request for data sent" },
        { Status::FrameCompleted, "a whole frame was received" }
    };

    auto iter = texts.find(status);
    if (iter == texts.end()) { return missing; }

    return iter->second;
}

void JkBmsSerial::announceStatus(JkBmsSerial::Status status)
{
    if (_lastStatus == status && millis() < _lastStatusPrinted + 10 * 1000) { return; }

    // after announcing once that the JK BMS is disabled by configuration, it
    // should just be silent while it is disabled.
    if (status == Status::DisabledByConfig && _lastStatus == status) { return; }

    MessageOutput.printf("[%11.3f] JK BMS: %s\r\n",
        static_cast<double>(millis())/1000, getStatusText(status).c_str());

    _lastStatus = status;
    _lastStatusPrinted = millis();
}

void JkBmsSerial::sendRequest()
{
    if ((millis() - _lastRequest) < _pollInterval * 1000) {
        return announceStatus(Status::WaitingForPollInterval);
    }

    if (!HwSerial.availableForWrite()) {
        return announceStatus(Status::HwSerialNotAvailableForWrite);
    }

    if (ReadState::Idle != _readState) {
        return announceStatus(Status::BusyReading);
    }

    JkBmsSerialMessage readAll(JkBmsSerialMessage::Command::ReadAll);
    HwSerial.write(readAll.data(), readAll.size());
    _lastRequest = millis();

    setReadState(ReadState::WaitingForFrameStart);
    return announceStatus(Status::RequestSent);
}

void JkBmsSerial::loop()
{
    sendRequest();

    while (HwSerial.available()) {
        rxData(HwSerial.read());
    }

    if (millis() > _lastRequest + 10 * 1000) {
        reset();
        return announceStatus(Status::Timeout);
    }
}

void JkBmsSerial::rxData(uint8_t inbyte)
{
    _buffer.push_back(inbyte);

    switch(_readState) {
        case ReadState::Idle: // unsolicited message from BMS
        case ReadState::WaitingForFrameStart:
            if (inbyte == 0x4E) {
                return setReadState(ReadState::FrameStartReceived);
            }
            break;
        case ReadState::FrameStartReceived:
            if (inbyte == 0x57) {
                return setReadState(ReadState::StartMarkerReceived);
            }
            break;
        case ReadState::StartMarkerReceived:
            _frameLength = inbyte << 8 | 0x00;
            return setReadState(ReadState::FrameLengthMsbReceived);
            break;
        case ReadState::FrameLengthMsbReceived:
            _frameLength |= inbyte;
            _frameLength -= 2; // length field already read
            return setReadState(ReadState::ReadingFrame);
            break;
        case ReadState::ReadingFrame:
            _frameLength--;
            if (_frameLength == 0) {
                return frameComplete();
            }
            return setReadState(ReadState::ReadingFrame);
            break;
    }

    reset();
}

void JkBmsSerial::reset()
{
    MessageOutput.println("resetting...");
    _buffer.clear();
    return setReadState(ReadState::Idle);
}

void JkBmsSerial::frameComplete()
{
    announceStatus(Status::FrameCompleted);

    for(auto const& b : _buffer) {
        MessageOutput.printf("%02x ", b);
    }
    MessageOutput.println("");
    auto pMsg = std::make_unique<JkBmsSerialMessage>(std::move(_buffer), _protocolVersion);
    if (pMsg->isValid()) {
        _pData = std::move(pMsg);
        _lastMessage = millis();

        auto oProtocolVersion = _pData->getDataPoints().get<JkBms::DataPointLabel::ProtocolVersion>();
        if (oProtocolVersion.has_value()) { _protocolVersion = *oProtocolVersion; }
    } // if invalid, error message has been produced by JkBmsSerialMessage
    reset();
}

bool JkBmsSerial::isDataValid() {
    return false; // TODO
}

} /* namespace JkBms */
