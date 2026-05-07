// SPDX-License-Identifier: GPL-2.0-or-later
#include <Arduino.h>
#include <Configuration.h>
#include <HardwareSerial.h>
#include <PinMapping.h>
#include <battery/pytes/rs485/Provider.h>
#include <battery/pytes/rs485/Stats.h>
#include <battery/pytes/rs485/HassIntegration.h>
#include <SerialPortManager.h>
#include <frozen/map.h>
#include <LogHelper.h>

#undef TAG
static const char* TAG = "battery";
static const char* SUBTAG = "Pytes RS485";

namespace Batteries::Pytes::Rs485 {

static constexpr uint8_t SOI = 0x7E;
static constexpr uint8_t EOI = 0x0D;

// ---------------------------------------------------------------------------
// Provider lifecycle
// ---------------------------------------------------------------------------

Provider::Provider()
    : _stats(std::make_shared<Stats>())
    , _hassIntegration(std::make_shared<HassIntegration>(_stats)) { }

bool Provider::init()
{
    std::string ifcType = "transceiver";
    if (Interface::Transceiver != getInterface()) { ifcType = "TTL-UART"; }
    DTU_LOGI("Initialize %s interface...", ifcType.c_str());

    const PinMapping_t& pin = PinMapping.get();
    DTU_LOGD("rx = %d, rxen = %d, tx = %d, txen = %d",
            pin.battery_rx, pin.battery_rxen, pin.battery_tx, pin.battery_txen);

    if (pin.battery_rx <= GPIO_NUM_NC || pin.battery_tx <= GPIO_NUM_NC) {
        DTU_LOGE("Invalid RX/TX pin config");
        return false;
    }

    auto oHwSerialPort = SerialPortManager.allocatePort(_serialPortOwner);
    if (!oHwSerialPort) { return false; }

    _upSerial = std::make_unique<HardwareSerial>(*oHwSerialPort);

    _upSerial->end();
    _upSerial->begin(9600, SERIAL_8N1, pin.battery_rx, pin.battery_tx);
    _upSerial->flush();

    if (Interface::Transceiver != getInterface()) { return true; }

    _rxEnablePin = pin.battery_rxen;
    _txEnablePin = pin.battery_txen;

    if (_rxEnablePin <= GPIO_NUM_NC || _txEnablePin <= GPIO_NUM_NC) {
        DTU_LOGE("Invalid transceiver pin config");
        return false;
    }

    pinMode(_rxEnablePin, OUTPUT);
    pinMode(_txEnablePin, OUTPUT);
    digitalWrite(_rxEnablePin, LOW);  // enable reception
    digitalWrite(_txEnablePin, LOW);  // disable transmission

    return true;
}

void Provider::deinit()
{
    _upSerial->end();

    if (_rxEnablePin > GPIO_NUM_NC) { pinMode(_rxEnablePin, INPUT); }
    if (_txEnablePin > GPIO_NUM_NC) { pinMode(_txEnablePin, INPUT); }

    SerialPortManager.freePort(_serialPortOwner);
}

Provider::Interface Provider::getInterface() const
{
    auto const& config = Configuration.get();
    if (0x00 == config.Battery.Serial.Interface) { return Interface::Uart; }
    if (0x01 == config.Battery.Serial.Interface) { return Interface::Transceiver; }
    return Interface::Invalid;
}

// ---------------------------------------------------------------------------
// Status logging
// ---------------------------------------------------------------------------

frozen::string const& Provider::getStatusText(Provider::Status status)
{
    static constexpr frozen::string missing = "programmer error: missing status text";

    static constexpr frozen::map<Status, frozen::string, 7> texts = {
        { Status::Initializing,                 "initializing" },
        { Status::Timeout,                      "timeout waiting for response from BMS" },
        { Status::WaitingForPollInterval,       "waiting for poll interval to elapse" },
        { Status::HwSerialNotAvailableForWrite, "UART is not available for writing" },
        { Status::BusyReading,                  "busy waiting for or reading a message from the BMS" },
        { Status::RequestSent,                  "request for data sent" },
        { Status::FrameCompleted,               "a whole frame was received" },
    };

    auto iter = texts.find(status);
    if (iter == texts.end()) { return missing; }
    return iter->second;
}

void Provider::announceStatus(Provider::Status status)
{
    if (_lastStatus == status && millis() < _lastStatusPrinted + 10 * 1000) { return; }
    DTU_LOGI("%s", getStatusText(status).data());
    _lastStatus = status;
    _lastStatusPrinted = millis();
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------

void Provider::loop()
{
    auto const& config = Configuration.get();
    uint8_t pollInterval = config.Battery.Serial.PollingInterval;

    while (_upSerial->available()) {
        rxData(static_cast<uint8_t>(_upSerial->read()));
    }

    sendRequest(pollInterval);

    if (_readState != ReadState::Idle &&
        millis() > _lastRequest + 2 * pollInterval * 1000 + 250) {
        reset();
        _queryStep = QueryStep::Done;
        announceStatus(Status::Timeout);
    }
}

// ---------------------------------------------------------------------------
// Request sending
// ---------------------------------------------------------------------------

void Provider::sendRequest(uint8_t pollInterval)
{
    // Only send a new poll when we have finished the previous cycle
    if (_queryStep != QueryStep::Done && _queryStep != QueryStep::PackBasic) {
        if (_readState != ReadState::Idle) {
            return announceStatus(Status::BusyReading);
        }
    }

    bool newCycle = false;
    if (_queryStep == QueryStep::Done) {
        if ((millis() - _lastCycleStart) < pollInterval * 1000) {
            return announceStatus(Status::WaitingForPollInterval);
        }
        // Start new cycle
        _queryStep = _firstPoll ? QueryStep::PackBasic : QueryStep::PackAnalog;
        newCycle = true;
    }

    if (_readState != ReadState::Idle) {
        return announceStatus(Status::BusyReading);
    }

    if (!_upSerial->availableForWrite()) {
        return announceStatus(Status::HwSerialNotAvailableForWrite);
    }

    bool sent = false;
    switch (_queryStep) {
        case QueryStep::PackBasic:
            sendCommand(SerialCommand::Command::PackBasic);
            sent = true;
            break;
        case QueryStep::ModuleBasic:
            sendCommand(SerialCommand::Command::ModuleBasic, { _currentModuleNo });
            sent = true;
            break;
        case QueryStep::ModuleProtect:
            sendCommand(SerialCommand::Command::ModuleProtect, { _currentModuleNo });
            sent = true;
            break;
        case QueryStep::PackAnalog:
            sendCommand(SerialCommand::Command::PackAnalog);
            sent = true;
            break;
        case QueryStep::PackChgDsg:
            sendCommand(SerialCommand::Command::PackChgDsg);
            sent = true;
            break;
        case QueryStep::ModuleAnalog:
            sendCommand(SerialCommand::Command::ModuleAnalog, { _currentModuleNo });
            sent = true;
            break;
        case QueryStep::ModuleChgDsg:
            sendCommand(SerialCommand::Command::ModuleChgDsg, { _currentModuleNo });
            sent = true;
            break;
        case QueryStep::ModuleCells:
            sendCommand(SerialCommand::Command::ModuleCells, { _currentModuleNo });
            sent = true;
            break;
        default:
            break;
    }

    if (!sent) { return; }
    if (newCycle) { _lastCycleStart = millis(); }
    _lastRequest = millis();
    setReadState(ReadState::WaitingForFrameStart);
    announceStatus(Status::RequestSent);
}

void Provider::sendCommand(SerialCommand::Command cmd, std::vector<uint8_t> info)
{
    // Address is fixed at 1 (cluster address from DIP switch); use 1 until
    // a dedicated config field is added.
    SerialCommand frame(cmd, 1, info);

    if (Interface::Transceiver == getInterface()) {
        digitalWrite(_rxEnablePin, HIGH); // disable reception while transmitting
        digitalWrite(_txEnablePin, HIGH); // enable transmission
    }

    _upSerial->write(frame.data(), frame.size());

    if (Interface::Transceiver == getInterface()) {
        _upSerial->flush();
        digitalWrite(_rxEnablePin, LOW); // enable reception
        digitalWrite(_txEnablePin, LOW); // disable transmission
    }
}

// ---------------------------------------------------------------------------
// Byte reception state machine
// ---------------------------------------------------------------------------

void Provider::rxData(uint8_t inbyte)
{
    switch (_readState) {
        case ReadState::Idle:
        case ReadState::WaitingForFrameStart:
            if (inbyte == SOI) {
                _rxBuffer.clear();
                _rxBuffer.push_back(inbyte);
                setReadState(ReadState::ReadingFrame);
            }
            return;

        case ReadState::ReadingFrame:
            _rxBuffer.push_back(inbyte);
            if (inbyte == EOI) {
                frameComplete();
            }
            return;
    }
}

void Provider::reset()
{
    _rxBuffer.clear();
    setReadState(ReadState::Idle);
}

// ---------------------------------------------------------------------------
// Frame validation and dispatch
// ---------------------------------------------------------------------------

void Provider::frameComplete()
{
    announceStatus(Status::FrameCompleted);

    SerialResponse response(_rxBuffer);
    reset();

    if (!response.isValid()) {
        _queryStep = QueryStep::Done;
        return;
    }

    if (response.rtn() != 0x00) {
        static constexpr frozen::map<uint8_t, frozen::string, 11> rtnNames = {
            { 0x01, "VER error" },
            { 0x02, "CHKSUM error" },
            { 0x03, "LENGTH CHKSUM error" },
            { 0x04, "CID2 invalid" },
            { 0x05, "format error" },
            { 0x06, "invalid data" },
            { 0x07, "ADR error" },
            { 0x08, "comm error" },
            { 0x09, "firmware checksum error" },
            { 0xFF, "not supported" },
            { 0x00, "ok" }, // padding entry
        };
        auto it = rtnNames.find(response.rtn());
        DTU_LOGW("BMS error response RTN=0x%02X (%s)", response.rtn(),
                 it != rtnNames.end() ? it->second.data() : "unknown");

        switch (_queryStep) {
            case QueryStep::PackBasic: {
                _currentModuleNo = 1;
                _queryStep = QueryStep::ModuleBasic;
                break;
            }
            case QueryStep::ModuleBasic: {
                if (++_currentModuleNo <= _numModules)
                    _queryStep = QueryStep::ModuleBasic;
                else {
                    _currentModuleNo = 1;
                    _queryStep = QueryStep::ModuleProtect;
                }
                break;
            }
            case QueryStep::ModuleProtect: {
                if (++_currentModuleNo <= _numModules)
                    _queryStep = QueryStep::ModuleProtect;
                else {
                    _currentModuleNo = 1;
                    _queryStep = QueryStep::PackAnalog;
                }
                break;
            }
            case QueryStep::PackAnalog: {
                _queryStep = QueryStep::PackChgDsg;
                break;
            }
            case QueryStep::PackChgDsg: {
                _currentModuleNo = 1;
                _queryStep = QueryStep::ModuleAnalog;
                break;
            }
            case QueryStep::ModuleAnalog: {
                if (++_currentModuleNo <= _numModules)
                    _queryStep = QueryStep::ModuleAnalog;
                else {
                    _currentModuleNo = 1;
                    _queryStep = QueryStep::ModuleChgDsg;
                }
                break;
            }
            case QueryStep::ModuleChgDsg: {
                if (++_currentModuleNo <= _numModules)
                    _queryStep = QueryStep::ModuleChgDsg;
                else {
                    _currentModuleNo = 1;
                    _queryStep = QueryStep::ModuleCells;
                }
                break;
            }
            case QueryStep::ModuleCells: {
                if (++_currentModuleNo <= _numModules)
                    _queryStep = QueryStep::ModuleCells;
                else
                    _queryStep = QueryStep::Done;
                break;
            }
            default: {
                _queryStep = QueryStep::Done;
                break;
            }
        }
        return;
    }

    switch (response.cid2()) {
        case 0x60: {
            auto dp = Parsers::parsePackBasic(response);
            _stats->updatePackData(dp);

            auto oCount = dp.get<DataPointLabel::ModuleCount>();
            if (oCount.has_value()) {
                _numModules = *oCount;
            }

            _firstPoll = false;
            _currentModuleNo = 1;
            _queryStep = QueryStep::ModuleBasic;
            break;
        }
        case 0x80: {
            auto r = Parsers::parseModuleBasic(response);
            _stats->setModuleBasic(r.moduleNo, r.hwVersion, r.swVersion, r.serial);

            if (++_currentModuleNo <= _numModules) {
                _queryStep = QueryStep::ModuleBasic;
            } else {
                _currentModuleNo = 1;
                _queryStep = QueryStep::ModuleProtect;
            }
            break;
        }
        case 0x82: {
            auto r = Parsers::parseModuleProtect(response);
            _stats->setModuleProtect(r.moduleNo, r.protect);

            if (++_currentModuleNo <= _numModules) {
                _queryStep = QueryStep::ModuleProtect;
            } else {
                _currentModuleNo = 1;
                _queryStep = QueryStep::PackAnalog;
            }
            break;
        }
        case 0x61: {
            auto dp = Parsers::parsePackAnalog(response);
            _stats->updatePackData(dp);

            _queryStep = QueryStep::PackChgDsg;
            break;
        }
        case 0x62: {
            auto dp = Parsers::parsePackChgDsg(response);
            _stats->updatePackData(dp);

            _currentModuleNo = 1;
            _queryStep = QueryStep::ModuleAnalog;
            break;
        }
        case 0x81: {
            auto r = Parsers::parseModuleAnalog(response);
            _stats->setModuleAnalog(r.moduleNo, r.voltageV, r.currentA, r.soc, r.health,
                                    r.totalCapacityMah, r.remainCapacityMah,
                                    r.ambientTemp, r.chargeCycles, r.balance,
                                    r.cellMaxV, r.cellMaxNo, r.cellMinV, r.cellMinNo,
                                    r.tempMaxC, r.tempMaxNo, r.tempMinC, r.tempMinNo);

            if (++_currentModuleNo <= _numModules) {
                _queryStep = QueryStep::ModuleAnalog;
            } else {
                _currentModuleNo = 1;
                _queryStep = QueryStep::ModuleChgDsg;
            }
            break;
        }
        case 0x83: {
            auto r = Parsers::parseModuleChgDsg(response);
            _stats->setModuleChgDsg(r.moduleNo, r.maxChgVoltV, r.minDsgVoltV,
                                        r.maxChgCurrA, r.maxDsgCurrA, r.fullChgReq, r.emergFlags);

            if (++_currentModuleNo <= _numModules) {
                _queryStep = QueryStep::ModuleChgDsg;
            } else {
                _currentModuleNo = 1;
                _queryStep = QueryStep::ModuleCells;
            }
            break;
        }
        case 0x92: {
            auto r = Parsers::parseModuleCells(response);
            _stats->setModuleCells(r.moduleNo, std::move(r.cells));

            if (++_currentModuleNo <= _numModules) {
                _queryStep = QueryStep::ModuleCells;
            } else {
                _queryStep = QueryStep::Done;
            }
            break;
        }
        default:
            DTU_LOGW("Unexpected CID2 0x%02X in response", response.cid2());
            _queryStep = QueryStep::Done;
            break;
    }

    // Immediately trigger next command in the cycle (no extra delay)
    if (_queryStep != QueryStep::Done) {
        auto const& config = Configuration.get();
        sendRequest(config.Battery.Serial.PollingInterval);
    }
}

} // namespace Batteries::Pytes::Rs485
