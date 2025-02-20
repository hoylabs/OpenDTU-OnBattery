// SPDX-License-Identifier: GPL-2.0-or-later
#include <solarcharger/victron/Provider.h>
#include "Configuration.h"
#include "PinMapping.h"
#include "MessageOutput.h"
#include "SerialPortManager.h"

namespace SolarChargers::Victron {

bool Provider::init(bool verboseLogging)
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

void Provider::deinit()
{
    std::lock_guard<std::mutex> lock(_mutex);

    _controllers.clear();
    for (auto const& o: _serialPortOwners) {
        SerialPortManager.freePort(o.c_str());
    }
    _serialPortOwners.clear();
}

bool Provider::initController(int8_t rx, int8_t tx, bool logging,
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

void Provider::setChargeLimit( float limit, float act_charge_current )
{
    _chargeLimit = limit;
    _chargeCurrent = act_charge_current;
}


void Provider::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);

    float overallChargeCurrent  { 0.0f };
    float overallLimit          { _chargeLimit };
    float reservedChargeCurrent { 0.5f };   // minimum current for a controller whenever the given limit is higher

    uint8_t numControllers { 0 };
    // calculate the actual charge current of all MPPTs
    for (auto const& upController : _controllers) {
        overallChargeCurrent += static_cast<float>( upController->getData().batteryCurrent_I_mA ) / 1000.0f;
        numControllers++;
    }
    // increase the charge limit with the current drawn by the inverter(s)
    float inverterCurrent { overallChargeCurrent - _chargeCurrent };
    overallLimit += inverterCurrent;

    // reserve a minimum charge-current for every controller, to ensure that we get a good distribution over all MPPTs
    const float overallReservedChargeCurrent { reservedChargeCurrent * static_cast<float>(numControllers) };
    if (overallLimit > overallReservedChargeCurrent) {
        overallLimit -= overallReservedChargeCurrent;
    } else {
        // the limit is lower than the needed reserve --> distribute the allowed limit in a simple way over all MPPTs
        reservedChargeCurrent = overallLimit / static_cast<float>(numControllers);
        overallLimit = 0.0f;
    }
    for (auto const& upController : _controllers) {
        const float batCurrent = static_cast<float>( upController->getData().batteryCurrent_I_mA ) / 1000.0f;
        const float factor = batCurrent / overallChargeCurrent;
        const float controllerLimit { factor * overallLimit + reservedChargeCurrent};

        upController->setChargeLimit(controllerLimit);
        upController->loop();

        if(upController->isDataValid()) {
            _stats->update(upController->getData().serialNr_SER, upController->getData(), upController->getLastUpdate());
        } else {
            _stats->update(upController->getData().serialNr_SER, std::nullopt, upController->getLastUpdate());
        }
    }
}

} // namespace SolarChargers::Victron
