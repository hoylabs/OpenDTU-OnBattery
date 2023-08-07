#include <Arduino.h>
#include "Configuration.h"
#include "HardwareSerial.h"
#include "PinMapping.h"
#include "MessageOutput.h"
#include "JkBmsDataPoints.h"
#include "JkBmsController.h"
#include <map>

HardwareSerial HwSerial(2);

namespace JkBms {

bool Controller::init()
{
    std::string ifcType = "transceiver";
    if (Interface::Transceiver != getInterface()) { ifcType = "TTL-UART"; }
    MessageOutput.printf("[JK BMS] Initialize %s interface...\r\n", ifcType.c_str());

    const PinMapping_t& pin = PinMapping.get();
    MessageOutput.printf("[JK BMS] rx = %d, rxen = %d, tx = %d, txen = %d\r\n",
            pin.battery_rx, pin.battery_rxen, pin.battery_tx, pin.battery_txen);

    if (pin.battery_rx < 0 || pin.battery_tx < 0) {
        MessageOutput.println(F("[JK BMS] Invalid RX/TX pin config"));
        return false;
    }

    HwSerial.begin(115200, SERIAL_8N1, pin.battery_rx, pin.battery_tx);
    HwSerial.flush();

    if (Interface::Transceiver != getInterface()) { return true; }

    _rxEnablePin = pin.battery_rxen;
    _txEnablePin = pin.battery_txen;

    if (_rxEnablePin < 0 || _txEnablePin < 0) {
        MessageOutput.println(F("[JK BMS] Invalid transceiver pin config"));
        return false;
    }

    pinMode(_rxEnablePin, OUTPUT);
    pinMode(_txEnablePin, OUTPUT);

    return true;
}

void Controller::deinit()
{
    HwSerial.end();

    if (_rxEnablePin > 0) { pinMode(_rxEnablePin, INPUT); }
    if (_txEnablePin > 0) { pinMode(_txEnablePin, INPUT); }
}

Controller::Interface Controller::getInterface() const
{
    CONFIG_T& config = Configuration.get();
    if (0x00 == config.Battery_JkBmsInterface) { return Interface::Uart; }
    if (0x01 == config.Battery_JkBmsInterface) { return Interface::Transceiver; }
    return Interface::Invalid;
}

std::string const& Controller::getStatusText(Controller::Status status)
{
    static const std::string missing =  "programmer error: missing status text";

    static const std::map<Status, const std::string> texts = {
        { Status::Timeout, "timeout wating for response from BMS" },
        { Status::WaitingForPollInterval, "waiting for poll interval to elapse" },
        { Status::HwSerialNotAvailableForWrite, "UART is not available for writing" },
        { Status::BusyReading, "busy waiting for or reading a message from the BMS" },
        { Status::RequestSent, "request for data sent" },
        { Status::FrameCompleted, "a whole frame was received" }
    };

    auto iter = texts.find(status);
    if (iter == texts.end()) { return missing; }

    return iter->second;
}

void Controller::announceStatus(Controller::Status status)
{
    if (_lastStatus == status && millis() < _lastStatusPrinted + 10 * 1000) { return; }

    MessageOutput.printf("[%11.3f] JK BMS: %s\r\n",
        static_cast<double>(millis())/1000, getStatusText(status).c_str());

    _lastStatus = status;
    _lastStatusPrinted = millis();
}

void Controller::sendRequest(uint8_t pollInterval)
{
    if (ReadState::Idle != _readState) {
        return announceStatus(Status::BusyReading);
    }

    if ((millis() - _lastRequest) < pollInterval * 1000) {
        return announceStatus(Status::WaitingForPollInterval);
    }

    if (!HwSerial.availableForWrite()) {
        return announceStatus(Status::HwSerialNotAvailableForWrite);
    }

    SerialCommand readAll(SerialCommand::Command::ReadAll);

    if (Interface::Transceiver == getInterface()) {
        digitalWrite(_rxEnablePin, HIGH); // disable reception (of our own data)
        digitalWrite(_txEnablePin, HIGH); // enable transmission
    }

    HwSerial.write(readAll.data(), readAll.size());

    if (Interface::Transceiver == getInterface()) {
        HwSerial.flush();
        digitalWrite(_rxEnablePin, LOW); // enable reception
        digitalWrite(_txEnablePin, LOW); // disable transmission (free the bus)
    }

    _lastRequest = millis();

    setReadState(ReadState::WaitingForFrameStart);
    return announceStatus(Status::RequestSent);
}

void Controller::loop()
{
    CONFIG_T& config = Configuration.get();
    uint8_t pollInterval = config.Battery_JkBmsPollingInterval;

    while (HwSerial.available()) {
        rxData(HwSerial.read());
    }

    sendRequest(pollInterval);

    if (millis() > _lastRequest + 2 * pollInterval * 1000 + 250) {
        reset();
        return announceStatus(Status::Timeout);
    }
}

void Controller::rxData(uint8_t inbyte)
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

void Controller::reset()
{
    _buffer.clear();
    return setReadState(ReadState::Idle);
}

void Controller::frameComplete()
{
    announceStatus(Status::FrameCompleted);

    for(auto const& b : _buffer) {
        MessageOutput.printf("%02x ", b);
    }
    MessageOutput.println("");
    auto pResponse = std::make_unique<SerialResponse>(std::move(_buffer), _protocolVersion);
    if (pResponse->isValid()) {
        processDataPoints(pResponse->getDataPoints());
    } // if invalid, error message has been produced by SerialResponse c'tor
    reset();
}

void Controller::processDataPoints(DataPointContainer const& dataPoints)
{
    _stats->updateFrom(dataPoints);

    using Label = JkBms::DataPointLabel;

    auto oProtocolVersion = dataPoints.get<Label::ProtocolVersion>();
    if (oProtocolVersion.has_value()) { _protocolVersion = *oProtocolVersion; }
}

} /* namespace JkBms */
