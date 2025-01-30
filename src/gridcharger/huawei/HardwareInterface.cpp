// SPDX-License-Identifier: GPL-2.0-or-later

#include <Arduino.h>
#include <Configuration.h>
#include <MessageOutput.h>
#include <gridcharger/huawei/HardwareInterface.h>

namespace GridCharger::Huawei {

void HardwareInterface::staticLoopHelper(void* context)
{
    auto pInstance = static_cast<HardwareInterface*>(context);
    static auto constexpr resetNotificationValue = pdTRUE;
    static auto constexpr notificationTimeout = pdMS_TO_TICKS(100);

    while (true) {
        ulTaskNotifyTake(resetNotificationValue, notificationTimeout);
        {
            std::unique_lock<std::mutex> lock(pInstance->_mutex);
            if (pInstance->_stopLoop) { break; }
            pInstance->loop();
        }
    }

    pInstance->_taskDone = true;

    vTaskDelete(nullptr);
}

bool HardwareInterface::startLoop()
{
    uint32_t constexpr stackSize = 3072;
    return pdPASS == xTaskCreate(HardwareInterface::staticLoopHelper,
            "HuaweiHwIfc", stackSize, this, 16/*prio*/, &_taskHandle);
}

void HardwareInterface::stopLoop()
{
    if (_taskHandle == nullptr) { return; }

    _taskDone = false;

    {
        std::unique_lock<std::mutex> lock(_mutex);
        _stopLoop = true;
    }

    xTaskNotifyGive(_taskHandle);

    while (!_taskDone) { delay(10); }
    _taskHandle = nullptr;
}

bool HardwareInterface::readBoardProperties(can_message_t const& msg)
{
    auto const& config = Configuration.get();

    // we process multiple messages with ID 0x1081D27F and a
    // single one with ID 0x181D27E, which concludes the answer.
    uint32_t constexpr lastId = 0x1081D27E;

    if ((msg.canId | 0x1) != (lastId | 0x1)) { return false; }

    uint16_t counter = static_cast<uint16_t>(msg.valueId >> 16);

    if (msg.canId == (lastId | 0x1) && counter == 1) {
        _boardProperties = "";
        _boardPropertiesCounter = 0;
        _boardPropertiesState = StringState::Reading;
    }

    if (StringState::Reading != _boardPropertiesState) { return true; }

    if (++_boardPropertiesCounter != counter) {
        MessageOutput.printf("[Huawei::HwIfc] missed message %d while reading "
                "board properties\r\n", _boardPropertiesCounter);
        _boardPropertiesState = StringState::MissedMessage;
        return true;
    }

    auto appendAscii = [this](uint32_t val) -> void {
        val &= 0xFF;
        if ((val < 0x20 || val > 0x7E) && val != 0x0A) { return; }
        // enforce CRLF line endings (as we do on all log lines)
        if (val == 0x0A) { _boardProperties.push_back('\r'); }
        _boardProperties.push_back(static_cast<char>(val));
    };

    appendAscii(msg.valueId >>  8);
    appendAscii(msg.valueId >>  0);
    appendAscii(msg.value   >> 24);
    appendAscii(msg.value   >> 16);
    appendAscii(msg.value   >>  8);
    appendAscii(msg.value   >>  0);

    if (msg.canId != lastId) { return true; }

    _boardPropertiesState = StringState::Complete;

    if (config.Huawei.VerboseLogging) {
        MessageOutput.printf("[Huawei::HwIfc] board properties:\r\n%s\r\n",
                _boardProperties.c_str());
    }

    auto getProperty = [this](std::string key) -> std::string {
        key += '=';
        auto start = _boardProperties.find(key);
        if (std::string::npos == start) { return "unknown"; }
        start += key.length();
        auto end = _boardProperties.find('\n', start);
        return _boardProperties.substr(start, end - start);
    };

    auto getDescription = [this,&getProperty](size_t idx) -> std::string {
        auto description = getProperty("Description");
        size_t start = 0;
        for (size_t i = 0; i < idx; ++i) {
            start = description.find(',', start);
            if (std::string::npos == start) { return ""; }
            ++start;
        }
        auto end = description.find(',', start);
        return description.substr(start, end - start);
    };

    _upData->add<DataPointLabel::BoardType>(getProperty("BoardType"));
    _upData->add<DataPointLabel::Serial>(getProperty("BarCode"));
    _upData->add<DataPointLabel::Manufactured>(getProperty("Manufactured"));
    _upData->add<DataPointLabel::VendorName>(getProperty("VendorName"));
    _upData->add<DataPointLabel::ProductName>(getDescription(1));

    std::string descr;
    size_t i = 2;
    do {
        descr = getDescription(i++);
    } while (!descr.empty() && descr.find("ectifier") == std::string::npos);
    _upData->add<DataPointLabel::ProductDescription>(descr);

    return true;
}

bool HardwareInterface::readRectifierState(can_message_t const& msg)
{
    auto const& config = Configuration.get();

    // we will receive a bunch of messages with CAN ID 0x1081407F,
    // and one (the last one) with ID 0x1081407E.
    if ((msg.canId | 0x1) != 0x1081407F) { return false; }

    uint32_t valueId = msg.valueId;

    // sometimes the last bit of the value ID of a message with CAN ID
    // 0x1081407E is set. TODO(schlimmchen): why?
    if (msg.canId == 0x1081407E && (valueId & 0x01) > 0) {
        if (config.Huawei.VerboseLogging) {
            MessageOutput.printf("[Huawei::HwIfc] last bit in value ID %08x is "
                    "set, resetting\r\n", valueId);
        }
        valueId &= ~(0x01);
    }

    // for unknown reasons, the input voltage value ID has the last two bits
    // set on a R4830G1. this unit supports DC input as well, but these bits
    // do not change when powered the unit using DC. TODO(schlimmchen): why?
    if (msg.canId == 0x1081407F && (valueId & 0x03) > 0) {
        if (config.Huawei.VerboseLogging) {
            MessageOutput.printf("[Huawei::HwIfc] last two bits in value ID "
                    "%08x are set, resetting\r\n", valueId);
        }
        valueId &= ~(0x03);
    }

    // during start-up and when shortening or opening the slot detect pins,
    // the value ID starts with 0x31 rather than 0x01. TODO(schlimmchen): why?
    if ((valueId >> 24) == 0x31) {
        if (config.Huawei.VerboseLogging) {
            MessageOutput.print("[Huawei::HwIfc] processing value for value ID "
                    "starting with 0x31\r\n");
        }
        valueId &= 0x0FFFFFFF;
    }

    if ((valueId & 0xFF00FFFF) != 0x01000000) { return false; }

    auto label = static_cast<DataPointLabel>((valueId & 0x00FF0000) >> 16);

    unsigned divisor = (label == DataPointLabel::OutputCurrentMax) ? _maxCurrentMultiplier : 1024;
    float value = static_cast<float>(msg.value)/divisor;
    switch (label) {
        case DataPointLabel::InputPower:
            _upData->add<DataPointLabel::InputPower>(value);
            break;
        case DataPointLabel::InputFrequency:
            _upData->add<DataPointLabel::InputFrequency>(value);
            break;
        case DataPointLabel::InputCurrent:
            _upData->add<DataPointLabel::InputCurrent>(value);
            break;
        case DataPointLabel::OutputPower:
            _upData->add<DataPointLabel::OutputPower>(value);
            break;
        case DataPointLabel::Efficiency:
            value *= 100;
            _upData->add<DataPointLabel::Efficiency>(value);
            break;
        case DataPointLabel::OutputVoltage:
            _upData->add<DataPointLabel::OutputVoltage>(value);
            break;
        case DataPointLabel::OutputCurrentMax:
            _upData->add<DataPointLabel::OutputCurrentMax>(value);
            break;
        case DataPointLabel::InputVoltage:
            _upData->add<DataPointLabel::InputVoltage>(value);
            break;
        case DataPointLabel::OutputTemperature:
            _upData->add<DataPointLabel::OutputTemperature>(value);
            break;
        case DataPointLabel::InputTemperature:
            _upData->add<DataPointLabel::InputTemperature>(value);
            break;
        case DataPointLabel::OutputCurrent:
            _upData->add<DataPointLabel::OutputCurrent>(value);
            break;
        default:
            uint8_t rawLabel = static_cast<uint8_t>(label);
            // 0x0E/0x0A seems to be a static label/value pair, so we don't print it
            if (config.Huawei.VerboseLogging && (rawLabel != 0x0E || msg.value != 0x0A)) {
                MessageOutput.printf("[Huawei::HwIfc] raw value for 0x%02x is "
                        "0x%08x (%d), scaled by 1024: %.2f, scaled by %d: %.2f\r\n",
                        rawLabel, msg.value, msg.value,
                        static_cast<float>(msg.value)/1024,
                        _maxCurrentMultiplier,
                        static_cast<float>(msg.value)/_maxCurrentMultiplier);
            }
            break;
    }

    return true;
}

void HardwareInterface::loop()
{
    can_message_t msg;
    auto const& config = Configuration.get();

    if (!_upData) { _upData = std::make_unique<DataPointContainer>(); }

    while (getMessage(msg)) {
        if (readBoardProperties(msg)) { continue; }

        if (readRectifierState(msg)) { continue; }

        // examples for codes not handled are:
        //     0x1081407E (Ack), 0x1081807E (Ack Frame),
        //     0x1001117E (Whr meter),
        //     0x100011FE (unclear), 0x108111FE (output enabled),
        //     0x108081FE (unclear).
        // https://github.com/craigpeacock/Huawei_R4850G2_CAN/blob/main/r4850.c
        // https://www.beyondlogic.org/review-huawei-r4850g2-power-supply-53-5vdc-3kw/
        if (config.Huawei.VerboseLogging) {
            MessageOutput.printf("[Huawei::HwIfc] ignoring message with CAN ID "
                    "0x%08x, value ID 0x%08x, and value 0x%08x\r\n",
                    msg.canId, msg.valueId, msg.value);
        }
    }

    if (StringState::Complete != _boardPropertiesState) {
        // stand by while processing the board properties replies and not timed out
        if (StringState::Reading == _boardPropertiesState &&
                _boardPropertiesRequestMillis + 5000 < millis()) { return; }

        static constexpr std::array<uint8_t, 8> data = { 0 };
        if (!sendMessage(0x1081D2FE, data)) {
            MessageOutput.print("[Huawei::HwIfc] Failed to send board properties request\r\n");
            _boardPropertiesState = StringState::RequestFailed;
        }
        _boardPropertiesRequestMillis = millis();

        return; // not sending other requests until we know the board properties
    }

    if (_nextRequestMillis < millis()) {
        _sendQueue.push(command_t {
            .tries = 1,
            .deviceAddress = 1,
            .registerAddress = 0x40FE,
            .command = 0,
            .flags = 0,
            .value = 0
        });

        _nextRequestMillis = millis() + DataRequestIntervalMillis;
    }

    size_t queueSize = _sendQueue.size();
    for (size_t i = 0; i < queueSize; ++i) {
        auto cmd = _sendQueue.front();
        _sendQueue.pop();

        std::array<uint8_t, 8> data = {
            static_cast<uint8_t>((cmd.command >> 8) & 0xFF),
            static_cast<uint8_t>((cmd.command >> 0) & 0xFF),
            static_cast<uint8_t>((cmd.flags >>  8) & 0xFF),
            static_cast<uint8_t>((cmd.flags >>  0) & 0xFF),
            static_cast<uint8_t>((cmd.value >> 24) & 0xFF),
            static_cast<uint8_t>((cmd.value >> 16) & 0xFF),
            static_cast<uint8_t>((cmd.value >>  8) & 0xFF),
            static_cast<uint8_t>((cmd.value >>  0) & 0xFF)
        };

        uint32_t addr = 0x10800000 | (cmd.deviceAddress << 16) | cmd.registerAddress;
        if (!sendMessage(addr, data)) {
            if (cmd.tries > 0) { --cmd.tries; }

            MessageOutput.printf("[Huawei::HwIfc] Sending to 0x%08x failed, "
                    "command 0x%04x, flags 0x%04x, value 0x%08x, %d tries remaining\r\n",
                    addr, cmd.command, cmd.flags, cmd.value, cmd.tries);

            if (cmd.tries > 0) { _sendQueue.push(cmd); }
        }
    }
}

void HardwareInterface::setFan(bool online, bool fullSpeed)
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_taskHandle == nullptr) { return; }

    _sendQueue.push(command_t {
        .tries = 3,
        .deviceAddress = 1,
        .registerAddress = 0x80FE,
        .command = static_cast<uint16_t>(online?0x0134:0x0135),
        .flags = static_cast<uint16_t>(fullSpeed?1:0),
        .value = 0
    });

    xTaskNotifyGive(_taskHandle);
}

void HardwareInterface::setProduction(bool enable)
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_taskHandle == nullptr) { return; }

    _sendQueue.push(command_t {
        .tries = 3,
        .deviceAddress = 1,
        .registerAddress = 0x80FE,
        .command = 0x0132,
        .flags = static_cast<uint16_t>(enable?0:1),
        .value = 0
    });

    _nextRequestMillis = millis() + 88; // request early param feedback

    xTaskNotifyGive(_taskHandle);
}

void HardwareInterface::setParameter(HardwareInterface::Setting setting, float val)
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_taskHandle == nullptr) { return; }

    uint16_t flags = 0;

    switch (setting) {
        case Setting::OfflineVoltage:
        case Setting::OnlineVoltage:
            val *= 1024;
            break;
        case Setting::OfflineCurrent:
        case Setting::OnlineCurrent:
            val *= _maxCurrentMultiplier;
            break;
        case Setting::InputCurrent:
            val *= 1024;
            if (val > 0) {
                flags = 0x0001;
            }
            break;
    }

    _sendQueue.push(command_t {
        .tries = 3,
        .deviceAddress = 1,
        .registerAddress = 0x80FE,
        .command = static_cast<uint16_t>(setting),
        .flags = flags,
        .value = static_cast<uint32_t>(val)
    });

    _nextRequestMillis = millis() + 88; // request early param feedback

    xTaskNotifyGive(_taskHandle);
}

std::unique_ptr<DataPointContainer> HardwareInterface::getCurrentData()
{
    std::unique_ptr<DataPointContainer> upData = nullptr;

    {
        std::lock_guard<std::mutex> lock(_mutex);
        upData = std::move(_upData);
    }

    auto const& config = Configuration.get();
    if (upData && config.Huawei.VerboseLogging) {
        auto iter = upData->cbegin();
        while (iter != upData->cend()) {
            MessageOutput.printf("[Huawei::HwIfc] [%.3f] %s: %s%s\r\n",
                static_cast<float>(iter->second.getTimestamp())/1000,
                iter->second.getLabelText().c_str(),
                iter->second.getValueText().c_str(),
                iter->second.getUnitText().c_str());
            ++iter;
        }
    }

    return std::move(upData);
}

} // namespace GridCharger::Huawei
