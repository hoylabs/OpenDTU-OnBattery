#include <Arduino.h>
#include "Configuration.h"
#include "HardwareSerial.h"
#include "MessageOutput.h"
#include "Battery.h"
#include "JkBmsSerial.h"
#include <map>

HardwareSerial HwSerial(2);

namespace JkBms {

JkBmsSerial Controller;

void JkBmsSerial::init(int8_t rx, int8_t rxEnableNot, int8_t tx, int8_t txEnable)
{
    HwSerial.begin(115200, SERIAL_8N1, rx, tx);
    HwSerial.flush();

    if (Interface::Transceiver != getInterface()) { return; }

    if (rxEnableNot < 0 || txEnable < 0) {
        return announceStatus(Status::InvalidTransceiverConfig);
    }

    _rxEnablePin = rxEnableNot;
    pinMode(rxEnableNot, OUTPUT);

    _txEnablePin = txEnable;
    pinMode(_txEnablePin, OUTPUT);
}

JkBmsSerial::Interface JkBmsSerial::getInterface() const
{
    CONFIG_T& config = Configuration.get();
    if (!config.Battery_Enabled) { return Interface::Disabled; }
    if (0x01 == config.Battery_Protocol) { return Interface::Uart; }
    if (0x02 == config.Battery_Protocol) { return Interface::Transceiver; }
    return Interface::Disabled;
}

std::string const& JkBmsSerial::getStatusText(JkBmsSerial::Status status)
{
    static const std::string missing =  "programmer error: missing status text";

    static const std::map<Status, const std::string> texts = {
        { Status::DisabledByConfig, "disabled by configuration" },
        { Status::InvalidTransceiverConfig, "invalid config: transceiver enable pins not specified" },
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
    if (ReadState::Idle != _readState) {
        return announceStatus(Status::BusyReading);
    }

    if ((millis() - _lastRequest) < _pollInterval * 1000) {
        return announceStatus(Status::WaitingForPollInterval);
    }

    if (!HwSerial.availableForWrite()) {
        return announceStatus(Status::HwSerialNotAvailableForWrite);
    }

    JkBmsSerialMessage readAll(JkBmsSerialMessage::Command::ReadAll);

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

void JkBmsSerial::loop()
{
    if (Interface::Disabled == getInterface()) {
        return announceStatus(Status::DisabledByConfig);
    }

    if (Interface::Transceiver == getInterface() && _txEnablePin < 0) {
        return announceStatus(Status::InvalidTransceiverConfig);
    }

    while (HwSerial.available()) {
        rxData(HwSerial.read());
    }

    sendRequest();

    if (millis() > _lastRequest + 2 * _pollInterval * 1000 + 250) {
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

        processDataPoints();
    } // if invalid, error message has been produced by JkBmsSerialMessage
    reset();
}

void JkBmsSerial::processDataPoints()
{
    using Label = JkBms::DataPointLabel;
    auto const& dataPoints = _pData->getDataPoints();

    auto oProtocolVersion = dataPoints.get<Label::ProtocolVersion>();
    if (oProtocolVersion.has_value()) { _protocolVersion = *oProtocolVersion; }

    Battery.lastUpdate = _lastMessage;

    auto oSoCValue = dataPoints.get<Label::BatterySoCPercent>();
    if (oSoCValue.has_value()) {
        Battery.stateOfCharge = *oSoCValue;
        auto oSoCDataPoint = dataPoints.getDataPointFor<Label::BatterySoCPercent>();
        Battery.stateOfChargeLastUpdate = oSoCDataPoint->getTimestamp();
    }

    auto oVoltage = dataPoints.get<Label::BatteryVoltageMilliVolt>();
    if (oVoltage.has_value()) { Battery.voltage = static_cast<float>(*oVoltage) / 1000; }

    auto oCurrent = dataPoints.get<Label::BatteryCurrentMilliAmps>();
    if (oCurrent.has_value()) { Battery.current = static_cast<float>(*oCurrent) / 1000; }

    auto oTemperature = dataPoints.get<Label::BatteryTempOneCelsius>();
    if (oTemperature.has_value()) { Battery.temperature = *oTemperature; }

    std::string manufacturer("JKBMS");
    auto oProductId = dataPoints.get<Label::ProductId>();
    if (oProductId.has_value()) {
        manufacturer = *oProductId;
        auto pos = oProductId->rfind("JK");
        if (pos != std::string::npos) {
            manufacturer = oProductId->substr(pos);
        }
    }
    strlcpy(Battery.manufacturer, manufacturer.c_str(), sizeof(Battery.manufacturer));

    auto oChargeEnabled = dataPoints.get<Label::BatteryChargeEnabled>();
    if (oChargeEnabled.has_value()) { Battery.chargeEnabled = *oChargeEnabled; }

    auto oDischargeEnabled = dataPoints.get<Label::BatteryDischargeEnabled>();
    if (oDischargeEnabled.has_value()) { Battery.dischargeEnabled = *oDischargeEnabled; }
}

bool JkBmsSerial::isDataValid() {
    return false; // TODO
}

} /* namespace JkBms */
