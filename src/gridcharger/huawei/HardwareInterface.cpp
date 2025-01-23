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
    static auto constexpr notificationTimeout = pdMS_TO_TICKS(500);

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

    return true;
}

bool HardwareInterface::readRectifierState(can_message_t const& msg)
{
    if (msg.canId != 0x1081407F) { return false; }

    if ((msg.valueId & 0xFF00FFFF) != 0x01000000) { return false; }

    if (!_upDataInFlight) { _upDataInFlight = std::make_unique<DataPointContainer>(); }

    auto label = static_cast<DataPointLabel>((msg.valueId & 0x00FF0000) >> 16);

    unsigned divisor = (label == DataPointLabel::OutputCurrentMax) ? _maxCurrentMultiplier : 1024;
    float value = static_cast<float>(msg.value)/divisor;
    switch (label) {
        case DataPointLabel::InputPower:
            _upDataInFlight->add<DataPointLabel::InputPower>(value);
            break;
        case DataPointLabel::InputFrequency:
            _upDataInFlight->add<DataPointLabel::InputFrequency>(value);
            break;
        case DataPointLabel::InputCurrent:
            _upDataInFlight->add<DataPointLabel::InputCurrent>(value);
            break;
        case DataPointLabel::OutputPower:
            _upDataInFlight->add<DataPointLabel::OutputPower>(value);
            break;
        case DataPointLabel::Efficiency:
            _upDataInFlight->add<DataPointLabel::Efficiency>(value);
            break;
        case DataPointLabel::OutputVoltage:
            _upDataInFlight->add<DataPointLabel::OutputVoltage>(value);
            break;
        case DataPointLabel::OutputCurrentMax:
            _upDataInFlight->add<DataPointLabel::OutputCurrentMax>(value);
            break;
        case DataPointLabel::InputVoltage:
            _upDataInFlight->add<DataPointLabel::InputVoltage>(value);
            break;
        case DataPointLabel::OutputTemperature:
            _upDataInFlight->add<DataPointLabel::OutputTemperature>(value);
            break;
        case DataPointLabel::InputTemperature:
            _upDataInFlight->add<DataPointLabel::InputTemperature>(value);
            break;
        case DataPointLabel::OutputCurrent:
            _upDataInFlight->add<DataPointLabel::OutputCurrent>(value);
            break;
    }

    // the OutputCurent value is the last value in a data request's answer
    // among all values we process into the data point container, so we
    // make the in-flight container the current container.
    if (label == DataPointLabel::OutputCurrent) {
        _upDataCurrent = std::move(_upDataInFlight);
    }

    return true;
}

void HardwareInterface::loop()
{
    can_message_t msg;
    auto const& config = Configuration.get();

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

    size_t queueSize = _sendQueue.size();
    for (size_t i = 0; i < queueSize; ++i) {
        auto [setting, val] = _sendQueue.front();
        _sendQueue.pop();

        std::array<uint8_t, 8> data = {
            0x01, static_cast<uint8_t>(setting), 0x00, 0x00,
            0x00, 0x00, static_cast<uint8_t>((val & 0xFF00) >> 8),
            static_cast<uint8_t>(val & 0xFF)
        };

        if (!sendMessage(0x108180FE, data)) {
            MessageOutput.print("[Huawei::HwIfc] Failed to set parameter\r\n");
            _sendQueue.push({setting, val});
        }
    }

    if (_nextRequestMillis < millis()) {
        static constexpr std::array<uint8_t, 8> data = { 0 };
        if (!sendMessage(0x108040FE, data)) {
            MessageOutput.print("[Huawei::HwIfc] Failed to send data request\r\n");
        }

        _nextRequestMillis = millis() + DataRequestIntervalMillis;

        // this should be redundant, as every answer to a data request should
        // have the OutputCurrent value, which is supposed to be the last value
        // in the answer, and it already triggers moving the data in flight.
        if (_upDataInFlight) {
            _upDataCurrent = std::move(_upDataInFlight);
        }
    }
}

void HardwareInterface::setParameter(HardwareInterface::Setting setting, float val)
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_taskHandle == nullptr) { return; }

    switch (setting) {
        case Setting::OfflineVoltage:
        case Setting::OnlineVoltage:
            val *= 1024;
            break;
        case Setting::OfflineCurrent:
        case Setting::OnlineCurrent:
            val *= _maxCurrentMultiplier;
            break;
    }

    _sendQueue.push({setting, static_cast<uint16_t>(val)});
    _nextRequestMillis = millis() - 1; // request param feedback immediately

    xTaskNotifyGive(_taskHandle);
}

std::unique_ptr<DataPointContainer> HardwareInterface::getCurrentData()
{
    std::unique_ptr<DataPointContainer> upData = nullptr;

    {
        std::lock_guard<std::mutex> lock(_mutex);
        upData = std::move(_upDataCurrent);
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
