// SPDX-License-Identifier: GPL-2.0-or-later
#include <invertermeter/Controller.h>
#include <Configuration.h>
#include <powermeter/Provider.h>
#include <powermeter/json/http/Provider.h>
#include <powermeter/json/mqtt/Provider.h>
#include <powermeter/sdm/serial/Provider.h>
#include <powermeter/sml/http/Provider.h>
#include <powermeter/sml/serial/Provider.h>
#include <powermeter/udp/smahm/Provider.h>
#include <powermeter/udp/victron/Provider.h>

InverterMeters::Controller InverterMeter;

namespace InverterMeters {

void Controller::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&Controller::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.enable();

    updateSettings();
}

void Controller::updateSettings()
{
    std::lock_guard<std::mutex> l(_mutex);

    if (_upProvider) { _upProvider.reset(); }

    auto const& imcfg = Configuration.get().InverterMeter;

    if (!imcfg.Enabled) { return; }

    // Reuse PowerMeter providers with InverterMeter configuration
    switch(static_cast<PowerMeters::Provider::Type>(imcfg.Source)) {
        case PowerMeters::Provider::Type::MQTT:
            _upProvider = std::make_unique<::PowerMeters::Json::Mqtt::Provider>(imcfg.Mqtt);
            break;
        case PowerMeters::Provider::Type::SDM1PH:
            _upProvider = std::make_unique<::PowerMeters::Sdm::Serial::Provider>(
                    ::PowerMeters::Sdm::Serial::Provider::Phases::One, imcfg.SerialSdm);
            break;
        case PowerMeters::Provider::Type::SDM3PH:
            _upProvider = std::make_unique<::PowerMeters::Sdm::Serial::Provider>(
                    ::PowerMeters::Sdm::Serial::Provider::Phases::Three, imcfg.SerialSdm);
            break;
        case PowerMeters::Provider::Type::HTTP_JSON:
            _upProvider = std::make_unique<::PowerMeters::Json::Http::Provider>(imcfg.HttpJson);
            break;
        case PowerMeters::Provider::Type::SERIAL_SML:
            _upProvider = std::make_unique<::PowerMeters::Sml::Serial::Provider>();
            break;
        case PowerMeters::Provider::Type::SMAHM2:
            _upProvider = std::make_unique<::PowerMeters::Udp::SmaHM::Provider>();
            break;
        case PowerMeters::Provider::Type::HTTP_SML:
            _upProvider = std::make_unique<::PowerMeters::Sml::Http::Provider>(imcfg.HttpSml);
            break;
        case PowerMeters::Provider::Type::UDP_VICTRON:
            _upProvider = std::make_unique<::PowerMeters::Udp::Victron::Provider>(imcfg.UdpVictron);
            break;
    }

    if (_upProvider && !_upProvider->init()) {
        _upProvider = nullptr;
    }
}

float Controller::getPowerTotal() const
{
    std::lock_guard<std::mutex> l(_mutex);
    if (!_upProvider) { return 0.0; }
    return _upProvider->getPowerTotal();
}

uint32_t Controller::getLastUpdate() const
{
    std::lock_guard<std::mutex> l(_mutex);
    if (!_upProvider) { return 0; }
    return _upProvider->getLastUpdate();
}

bool Controller::isDataValid() const
{
    std::lock_guard<std::mutex> l(_mutex);
    if (!_upProvider) { return false; }
    return _upProvider->isDataValid();
}

void Controller::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_upProvider) { return; }
    _upProvider->loop();

    // Skip MQTT republishing for InverterMeter to avoid conflicts
}

} // namespace InverterMeters