// SPDX-License-Identifier: GPL-2.0-or-later
#include "SolarCharger.h"
#include <Configuration.h>
#include <MessageOutput.h>
#include <VictronMppt.h>

SolarChargerClass SolarCharger;

void SolarChargerClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&SolarChargerClass::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.enable();

    this->updateSettings();
}

void SolarChargerClass::updateSettings()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        _upProvider->deinit();
        _upProvider = nullptr;
    }

    auto const& config = Configuration.get();
    if (!config.SolarCharger.Enabled) { return; }

    bool verboseLogging = config.SolarCharger.VerboseLogging;

    switch (config.SolarCharger.Provider) {
        case SolarChargerProviderType::VEDIRECT:
            _upProvider = std::make_unique<VictronMppt>();
            break;
        default:
            MessageOutput.printf("[SolarCharger] Unknown provider: %d\r\n", config.SolarCharger.Provider);
            return;
    }

    if (!_upProvider->init(verboseLogging)) { _upProvider = nullptr; }
}

void SolarChargerClass::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        _upProvider->loop();
    }
}

size_t SolarChargerClass::controllerAmount() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        return _upProvider->controllerAmount();
    }

    return 0;
}

bool SolarChargerClass::isDataValid() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        return _upProvider->isDataValid();
    }

    return false;
}

uint32_t SolarChargerClass::getDataAgeMillis() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        return _upProvider->getDataAgeMillis();
    }

    return 0;
}

uint32_t SolarChargerClass::getDataAgeMillis(size_t idx) const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        return _upProvider->getDataAgeMillis(idx);
    }

    return 0;
}


// total output of all MPPT charge controllers in Watts
int32_t SolarChargerClass::getOutputPowerWatts() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        return _upProvider->getOutputPowerWatts();
    }

    return 0;
}

// total panel input power of all MPPT charge controllers in Watts
int32_t SolarChargerClass::getPanelPowerWatts() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        return _upProvider->getPanelPowerWatts();
    }

    return 0;
}

// sum of total yield of all MPPT charge controllers in kWh
float SolarChargerClass::getYieldTotal() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        return _upProvider->getYieldTotal();
    }

    return 0;
}

// sum of today's yield of all MPPT charge controllers in kWh
float SolarChargerClass::getYieldDay() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        return _upProvider->getYieldDay();
    }

    return 0;
}

// minimum of all MPPT charge controllers' output voltages in V
float SolarChargerClass::getOutputVoltage() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        return _upProvider->getOutputVoltage();
    }

    return 0;
}

std::optional<VeDirectMpptController::data_t> SolarChargerClass::getData(size_t idx) const
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        return _upProvider->getData(idx);
    }

    return std::nullopt;
}
