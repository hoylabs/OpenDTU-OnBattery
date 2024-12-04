// SPDX-License-Identifier: GPL-2.0-or-later
#include "VictronMppt.h"
#include "Configuration.h"
#include "PinMapping.h"
#include "MessageOutput.h"
#include "SerialPortManager.h"

bool VictronMppt::init(bool verboseLogging)
{
    const PinMapping_t& pin = PinMapping.get();
    auto controllerCount = 0;

    if (initController(pin.victron_rx, pin.victron_tx, verboseLogging, 1)) {
        controllerCount++;
    }

    if (initController(pin.victron_rx2, pin.victron_tx2, verboseLogging, 2)) {
        controllerCount++;
    }

    if (initController(pin.victron_rx3, pin.victron_tx3, verboseLogging, 3)) {
        controllerCount++;
    }

    return controllerCount > 0;
}

void VictronMppt::deinit()
{
    std::lock_guard<std::mutex> lock(_mutex);

    _controllers.clear();
    for (auto const& o: _serialPortOwners) {
        SerialPortManager.freePort(o.c_str());
    }
    _serialPortOwners.clear();
}

bool VictronMppt::initController(int8_t rx, int8_t tx, bool logging,
        uint8_t instance)
{
    MessageOutput.printf("[VictronMppt Instance %d] rx = %d, tx = %d\r\n",
            instance, rx, tx);

    if (rx < 0) {
        MessageOutput.printf("[VictronMppt Instance %d] invalid pin config\r\n", instance);
        return false;
    }

    String owner("Victron MPPT ");
    owner += String(instance);
    auto oHwSerialPort = SerialPortManager.allocatePort(owner.c_str());
    if (!oHwSerialPort) { return false; }

    _serialPortOwners.push_back(owner);

    auto upController = std::make_unique<VeDirectMpptController>();
    upController->init(rx, tx, &MessageOutput, logging, *oHwSerialPort);
    _controllers.push_back(std::move(upController));
    return true;
}

void VictronMppt::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);

    for (auto const& upController : _controllers) {
        upController->loop();
    }
}

/*
 * isDataValid()
 * return: true = if at least one of the MPPT controllers delivers valid data
 */
bool VictronMppt::isDataValid() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    for (auto const& upController: _controllers) {
        if (upController->isDataValid()) { return true; }
    }

    return false;
}

uint32_t VictronMppt::getDataAgeMillis() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_controllers.empty()) { return 0; }

    auto now = millis();

    auto iter = _controllers.cbegin();
    uint32_t age = now - (*iter)->getLastUpdate();
    ++iter;

    while (iter != _controllers.end()) {
        age = std::min<uint32_t>(age, now - (*iter)->getLastUpdate());
        ++iter;
    }

    return age;
}

uint32_t VictronMppt::getDataAgeMillis(size_t idx) const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_controllers.empty() || idx >= _controllers.size()) { return 0; }

    return millis() - _controllers[idx]->getLastUpdate();
}

std::optional<VeDirectMpptController::data_t> VictronMppt::getData(size_t idx) const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_controllers.empty() || idx >= _controllers.size()) {
        MessageOutput.printf("ERROR: MPPT controller index %d is out of bounds (%d controllers)\r\n",
                             idx, _controllers.size());
        return std::nullopt;
    }

    if (!_controllers[idx]->isDataValid()) { return std::nullopt; }

    return _controllers[idx]->getData();
}

int32_t VictronMppt::getOutputPowerWatts() const
{
    int32_t sum = 0;

    for (const auto& upController : _controllers) {
        if (!upController->isDataValid()) { continue; }

        // if any charge controller is part of a VE.Smart network, and if the
        // charge controller is connected in a way that allows to send
        // requests, we should have the "network total DC input power"
        // available. if so, to estimate the output power, we multiply by
        // the calculated efficiency of the connected charge controller.
        auto networkPower = upController->getData().NetworkTotalDcInputPowerMilliWatts;
        if (networkPower.first > 0) {
            return static_cast<int32_t>(networkPower.second / 1000.0 * upController->getData().mpptEfficiency_Percent / 100);
        }

        sum += upController->getData().batteryOutputPower_W;
    }

    return sum;
}

int32_t VictronMppt::getPanelPowerWatts() const
{
    int32_t sum = 0;

    for (const auto& upController : _controllers) {
        if (!upController->isDataValid()) { continue; }

        // if any charge controller is part of a VE.Smart network, and if the
        // charge controller is connected in a way that allows to send
        // requests, we should have the "network total DC input power" available.
        auto networkPower = upController->getData().NetworkTotalDcInputPowerMilliWatts;
        if (networkPower.first > 0) {
            return static_cast<int32_t>(networkPower.second / 1000.0);
        }

        sum += upController->getData().panelPower_PPV_W;
    }

    return sum;
}

float VictronMppt::getYieldTotal() const
{
    float sum = 0;

    for (const auto& upController : _controllers) {
        if (!upController->isDataValid()) { continue; }
        sum += upController->getData().yieldTotal_H19_Wh / 1000.0;
    }

    return sum;
}

float VictronMppt::getYieldDay() const
{
    float sum = 0;

    for (const auto& upController : _controllers) {
        if (!upController->isDataValid()) { continue; }
        sum += upController->getData().yieldToday_H20_Wh / 1000.0;
    }

    return sum;
}

float VictronMppt::getOutputVoltage() const
{
    float min = -1;

    for (const auto& upController : _controllers) {
        if (!upController->isDataValid()) { continue; }
        float volts = upController->getData().batteryVoltage_V_mV / 1000.0;
        if (min == -1) { min = volts; }
        min = std::min(min, volts);
    }

    return min;
}

std::optional<uint8_t> VictronMppt::getStateOfOperation() const
{
    for (const auto& upController : _controllers) {
        if (upController->isDataValid()) {
            return upController->getData().currentState_CS;
        }
    }

    return std::nullopt;
}

std::optional<float> VictronMppt::getVoltage(MPPTVoltage kindOf) const
{
    for (const auto& upController : _controllers) {
        switch (kindOf) {
            case MPPTVoltage::ABSORPTION: {
                auto const& absorptionVoltage = upController->getData().BatteryAbsorptionMilliVolt;
                if (absorptionVoltage.first > 0) { return absorptionVoltage.second; }
                break;
            }
            case MPPTVoltage::FLOAT: {
                auto const& floatVoltage = upController->getData().BatteryFloatMilliVolt;
                if (floatVoltage.first > 0) { return floatVoltage.second; }
                break;
            }
            case MPPTVoltage::BATTERY: {
                auto const& batteryVoltage = upController->getData().batteryVoltage_V_mV;
                if (upController->isDataValid()) { return batteryVoltage; }
                break;
            }
        }
    }

    return std::nullopt;
}
