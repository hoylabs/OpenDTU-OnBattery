// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>
#include <mutex>
#include <TaskSchedulerDeclarations.h>
#include <solarcharger/Provider.h>
#include <VeDirectMpptController.h>

namespace SolarChargers {

class Controller {
public:
    void init(Scheduler&);
    void updateSettings();

    // TODO(andreasboehm): below methods are taken from VictronMppt to start abstracting
    // solar chargers without breaking everything.
    size_t controllerAmount();
    uint32_t getDataAgeMillis();
    uint32_t getDataAgeMillis(size_t idx);

    // total output of all MPPT charge controllers in Watts
    int32_t getOutputPowerWatts();

    // total panel input power of all MPPT charge controllers in Watts
    int32_t getPanelPowerWatts();

    // sum of total yield of all MPPT charge controllers in kWh
    float getYieldTotal();

    // sum of today's yield of all MPPT charge controllers in kWh
    float getYieldDay();

    // minimum of all MPPT charge controllers' output voltages in V
    float getOutputVoltage();

    std::optional<VeDirectMpptController::data_t> getData(size_t idx = 0);

    bool isDataValid();

private:
    void loop();

    Task _loopTask;
    mutable std::mutex _mutex;
    std::unique_ptr<Provider> _upProvider = nullptr;
    bool _publishSensors = false;
};

} // namespace SolarChargers

extern SolarChargers::Controller SolarCharger;
