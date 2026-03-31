// SPDX-License-Identifier: GPL-2.0-or-later

/* Inverter-Meter
 *
 * The Inverter-Meter adding support for an AC power meter to measure inverter output power for DPL functionality.
 * During the DPL loop the value from the Inverter-Meter is used and not the value from the inverter internal power meter.
 * Especially HM-xxx inverters powered by 24VDC measure wrong power values at high loads.
 * - reuse existing PowerMeter providers to get the inverter power values
 *
 * Currently restrictions:
 * - only one Inverter-Meter supported
 * - only one phase power measurement supported
 *
 * 01.08.2024 - 0.1 - first version
 */

#include <invertermeter/Controller.h>
#include <Configuration.h>
#include <powermeter/json/http/Provider.h>
#include <powermeter/json/mqtt/Provider.h>
#include <powermeter/sdm/serial/Provider.h>
#include <powermeter/sml/http/Provider.h>
#include <powermeter/sml/serial/Provider.h>
#include <powermeter/smahm/udp/Provider.h>
#include <powermeter/modbus/udp/victron/Provider.h>

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
            _upProvider = std::make_unique<::PowerMeters::SmaHM::Udp::Provider>();
            break;
        case PowerMeters::Provider::Type::HTTP_SML:
            _upProvider = std::make_unique<::PowerMeters::Sml::Http::Provider>(imcfg.HttpSml);
            break;
        case PowerMeters::Provider::Type::MODBUS_UDP_VICTRON:
            _upProvider = std::make_unique<::PowerMeters::Modbus::Udp::Victron::Provider>(imcfg.UdpVictron);
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

std::optional<float> Controller::getPower(uint64_t inverterID) const
{
    // for now we only support one inverter meter
    auto serial = Configuration.get().InverterMeter.Serial;

    std::lock_guard<std::mutex> l(_mutex);

    if (inverterID != serial) { return std::nullopt; }

    if (!_upProvider) { return std::nullopt; }

    if (!_upProvider->isDataValid()) { return std::nullopt; }

    return _upProvider->getPowerTotal();
}

uint32_t Controller::getTime(uint64_t inverterSN) const
{
    // for now we only support one inverter meter
    auto serial = Configuration.get().InverterMeter.Serial;

    std::lock_guard<std::mutex> l(_mutex);

    if (inverterSN != serial) { return 0; }
    if (!_upProvider) { return 0; }

    return _upProvider->getLastUpdate();
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

uint32_t Controller::getRequestTime() const
{
    std::lock_guard<std::mutex> l(_mutex);
    if (_upProvider) {
        if (Configuration.get().InverterMeter.Source == static_cast<uint32_t>(PowerMeters::Provider::Type::HTTP_JSON)) {
            return Configuration.get().InverterMeter.HttpJson.PollingInterval + 200;
        }
        if (Configuration.get().InverterMeter.Source == static_cast<uint32_t>(PowerMeters::Provider::Type::HTTP_SML)) {
            return Configuration.get().InverterMeter.HttpSml.PollingInterval + 200;
        }
    }
    return 2000;
}

void Controller::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_upProvider) { return; }
    _upProvider->loop();

    // Skip MQTT republishing for InverterMeter to avoid conflicts
}

} // namespace InverterMeters
