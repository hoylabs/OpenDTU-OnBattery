// SPDX-License-Identifier: GPL-2.0-or-later
#include <solarcharger/victron/Provider.h>
#include "Configuration.h"
#include "PinMapping.h"
#include "SerialPortManager.h"
#include <LogHelper.h>

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

void Provider::setChargeLimit( float limit, float act_charge_current )
{
    _chargeLimit = limit;
    _chargeCurrent = act_charge_current;
}


void Provider::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);

    float overallChargeCurrent  { 0.0f };
    float remainingLimit        { _chargeLimit };
    float reservedChargeCurrent { 0.5f };   // minimum current for a controller whenever the given limit is higher

    uint8_t numControllers { 0 };
    // calculate the actual charge current of all MPPTs
    for (auto const& upController : _controllers) {
        overallChargeCurrent += static_cast<float>( upController->getData().batteryCurrent_I_mA ) / 1000.0f;
        numControllers++;
    }
    // increase the charge limit with the current drawn by the inverter(s)
    const float inverterCurrent { overallChargeCurrent - _chargeCurrent };
	if (inverterCurrent >= 0.0f) {
		remainingLimit += inverterCurrent;
	}

    // reserve a minimum charge-current for every controller, to ensure that we get a good distribution over all MPPTs
    const float overallReservedChargeCurrent { reservedChargeCurrent * static_cast<float>(numControllers) };
    if (remainingLimit > overallReservedChargeCurrent) {
        remainingLimit -= overallReservedChargeCurrent;
    } else {
        // the limit is lower than the needed reserve --> distribute the allowed limit in a simple way over all MPPTs
        reservedChargeCurrent = remainingLimit / static_cast<float>(numControllers);
        remainingLimit = 0.0f;
    }
    for (auto const& upController : _controllers) {
        const float batCurrent { static_cast<float>( upController->getData().batteryCurrent_I_mA ) / 1000.0f };
        float factor { 0.0f };
        // if there is not current for automatic distribution --> distribute the limit uniformly to all controllers
        if (overallChargeCurrent <= 0.0f) {
            factor = 1.0 / static_cast<float>(numControllers);
        } else {
            factor = batCurrent / overallChargeCurrent;
        }
        float controllerLimit { reservedChargeCurrent};
        // no limit left for this controller? Only apply the absolute minimum
        if (remainingLimit > 0.0f) {
            controllerLimit += factor * remainingLimit;
        }
        // get the maximum allowed battery current of the charger
        auto batMaxCurrent { upController->getData().BatteryMaximumCurrent };
        // is the data valid?
        if (batMaxCurrent.first > 0) {
            const float maxControllerCurrent { static_cast<float>( batMaxCurrent.second ) / 10.0f };
            // limit to the maximum allowed battery current of the charger
            if (controllerLimit > maxControllerCurrent) {
                controllerLimit = maxControllerCurrent;
            }
        }
        // substract the set limit from the remaining limit for distribution to the other chargers
        remainingLimit -= controllerLimit;
        overallChargeCurrent -= batCurrent;

        upController->setChargeLimit(controllerLimit);
        upController->loop();

        if (upController->isDataValid()) {
            _stats->update(upController->getLogId(), upController->getData(), upController->getLastUpdate());
        }
    }
}

} // namespace SolarChargers::Victron
