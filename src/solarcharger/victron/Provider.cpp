// SPDX-License-Identifier: GPL-2.0-or-later
#include <solarcharger/victron/Provider.h>
#include <solarcharger/ChargeCurrentDistributor.h>
#include <battery/Controller.h>
#include "Configuration.h"
#include "PinMapping.h"
#include "SerialPortManager.h"
#include <LogHelper.h>

#undef TAG
static const char* TAG = "solarCharger";
static const char* SUBTAG = "VE.Direct";

namespace SolarChargers::Victron {

bool Provider::init()
{
    const PinMapping_t& pin = PinMapping.get();
    auto controllerCount = 0;

    if (initController(pin.victron_rx, pin.victron_tx, 1)) {
        controllerCount++;
    }

    if (initController(pin.victron_rx2, pin.victron_tx2, 2)) {
        controllerCount++;
    }

    if (initController(pin.victron_rx3, pin.victron_tx3, 3)) {
        controllerCount++;
    }

    return controllerCount > 0;
}

void Provider::deinit()
{
    std::lock_guard<std::mutex> lock(_mutex);

    _controllers.clear();
    for (auto const& o: _serialPortOwners) {
        SerialPortManager.freePort(o.c_str());
    }
    _serialPortOwners.clear();
}

bool Provider::initController(gpio_num_t rx, gpio_num_t tx, uint8_t instance)
{
    DTU_LOGI("Instance %d: rx = %d, tx = %d", instance, rx, tx);

    if (rx <= GPIO_NUM_NC) {
        DTU_LOGE("Instance %d: invalid pin config", instance);
        return false;
    }

    auto const& config = Configuration.get();
    auto forwardBatteryData = config.SolarCharger.ForwardBatteryData;

    if (tx <= GPIO_NUM_NC && forwardBatteryData) {
        DTU_LOGE("Instance %d: TX pin not configured but forwaredBatteryData enabled", instance);
    }

    String owner("Victron MPPT ");
    owner += String(instance);
    auto oHwSerialPort = SerialPortManager.allocatePort(owner.c_str());
    if (!oHwSerialPort) { return false; }

    _serialPortOwners.push_back(owner);

    auto upController = std::make_unique<VeDirectMpptController>();
    upController->init(rx, tx, *oHwSerialPort);
    _controllers.push_back(std::move(upController));
    return true;
}

void Provider::loop()
{
    auto const& config = Configuration.get();
    auto const forwardBatteryData = config.SolarCharger.ForwardBatteryData;
    auto const batteryEnabled = config.Battery.Enabled;
    auto const chargeLimit = Battery.getChargeCurrentLimit();
    auto const limitActive = (chargeLimit != FLT_MAX);

    std::shared_ptr<::Batteries::Stats const> batteryStats;
    auto chargeCurrent = 0.0f;

    if (batteryEnabled) {
        batteryStats = Battery.getStats();
        chargeCurrent = batteryStats->getChargeCurrent();
    }

    std::lock_guard<std::mutex> lock(_mutex);

    if (limitActive) {
        applyChargeCurrentLimit(chargeLimit, chargeCurrent);
    } else if (_chargeLimitActive) {
        // transition: release any previously set limit in the MPPTs (once)
        for (auto const& upController : _controllers) {
            upController->setChargeLimit(FLT_MAX);
        }
        _chargeLimitActive = false;
    }

    for (auto const& upController : _controllers) {
        if (forwardBatteryData && batteryEnabled) {
            if (batteryStats->isVoltageValid() && batteryStats->getVoltageAgeSeconds() < 60) {
                upController->setRemoteVoltage(batteryStats->getVoltage());
            }

            auto oTemperature = batteryStats->getTemperature();
            if (oTemperature.has_value() && batteryStats->getTemperatureAgeSeconds() < 60) {
                upController->setRemoteTemperature(*oTemperature);
            }
        }

        upController->loop();

        if (upController->isDataValid()) {
            _stats->update(upController->getLogId(), upController->getData(), upController->getLastUpdate());
        }
    }
}

void Provider::applyChargeCurrentLimit(float chargeLimit, float chargeCurrent)
{
    std::vector<ChargeCurrentDistributor::ControllerData> data;
    data.reserve(_controllers.size());

    for (auto const& upController : _controllers) {
        auto const& mpptData = upController->getData();

        std::optional<float> maxCurrent;
        if (mpptData.BatteryMaximumCurrent.first > 0) {
            maxCurrent = static_cast<float>(mpptData.BatteryMaximumCurrent.second) / 10.0f;
        }

        std::optional<float> previousLimit;
        if (mpptData.ChargeCurrentLimit.first > 0) {
            previousLimit = static_cast<float>(mpptData.ChargeCurrentLimit.second) / 10.0f;
        }

        data.push_back({
            static_cast<float>(mpptData.batteryCurrent_I_mA) / 1000.0f,
            maxCurrent,
            previousLimit
        });
    }

    auto const limits = ChargeCurrentDistributor::distribute(chargeLimit, chargeCurrent, data);

    for (size_t i = 0; i < _controllers.size(); ++i) {
        _controllers[i]->setChargeLimit(limits[i]);
    }

    _chargeLimitActive = true;
}

} // namespace SolarChargers::Victron
