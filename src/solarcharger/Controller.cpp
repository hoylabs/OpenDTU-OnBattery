// SPDX-License-Identifier: GPL-2.0-or-later
#include <Configuration.h>
#include <MessageOutput.h>
#include <MqttSettings.h>
#include <solarcharger/Controller.h>
#include <solarcharger/victron/Provider.h>

SolarChargers::Controller SolarCharger;

namespace SolarChargers {

void Controller::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&Controller::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.enable();

    this->updateSettings();
}

void Controller::updateSettings()
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
            _upProvider = std::make_unique<::SolarChargers::Victron::Provider>();
            break;
        default:
            MessageOutput.printf("[SolarCharger] Unknown provider: %d\r\n", config.SolarCharger.Provider);
            return;
    }

    if (!_upProvider->init(verboseLogging)) { _upProvider = nullptr; }

    _publishSensors = true;
}

void Controller::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (!_upProvider) { return; }

    _upProvider->loop();

    _upProvider->getStats()->mqttLoop();

    auto const& config = Configuration.get();
    if (!config.Mqtt.Hass.Enabled) { return; }

    // TODO(schlimmchen): this cannot make sure that transient
    // connection problems are actually always noticed.
    if (!MqttSettings.getConnected()) {
        _publishSensors = true;
        return;
    }

    if (!_publishSensors) { return; }

    _upProvider->getHassIntegration().publishSensors();

    _publishSensors = false;
}

size_t Controller::controllerAmount()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        return _upProvider->controllerAmount();
    }

    return 0;
}

bool Controller::isDataValid()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        return _upProvider->isDataValid();
    }

    return false;
}

uint32_t Controller::getDataAgeMillis()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        return _upProvider->getDataAgeMillis();
    }

    return 0;
}

uint32_t Controller::getDataAgeMillis(size_t idx)
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        return _upProvider->getDataAgeMillis(idx);
    }

    return 0;
}


// total output of all MPPT charge controllers in Watts
int32_t Controller::getOutputPowerWatts()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        return _upProvider->getOutputPowerWatts();
    }

    return 0;
}

// total panel input power of all MPPT charge controllers in Watts
int32_t Controller::getPanelPowerWatts()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        return _upProvider->getPanelPowerWatts();
    }

    return 0;
}

// sum of total yield of all MPPT charge controllers in kWh
float Controller::getYieldTotal()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        return _upProvider->getYieldTotal();
    }

    return 0;
}

// sum of today's yield of all MPPT charge controllers in kWh
float Controller::getYieldDay()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        return _upProvider->getYieldDay();
    }

    return 0;
}

// minimum of all MPPT charge controllers' output voltages in V
float Controller::getOutputVoltage()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        return _upProvider->getOutputVoltage();
    }

    return 0;
}

std::optional<VeDirectMpptController::data_t> Controller::getData(size_t idx)
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_upProvider) {
        return _upProvider->getData(idx);
    }

    return std::nullopt;
}

} // namespace SolarChargers
