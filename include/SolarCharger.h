// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>
#include <mutex>
#include <TaskSchedulerDeclarations.h>
#include "SolarChargerProvider.h"
#include "VeDirectMpptController.h"

class SolarChargerClass {
public:
    void init(Scheduler&);
    void updateSettings();

    // TODO(andreasboehm): below methods are taken from VictronMppt to start abstracting
    // solar chargers without breaking everything.
    size_t controllerAmount() const;
    uint32_t getDataAgeMillis() const;
    uint32_t getDataAgeMillis(size_t idx) const;

    // total output of all MPPT charge controllers in Watts
    int32_t getOutputPowerWatts() const;

    // total panel input power of all MPPT charge controllers in Watts
    int32_t getPanelPowerWatts() const;

    // sum of total yield of all MPPT charge controllers in kWh
    float getYieldTotal() const;

    // sum of today's yield of all MPPT charge controllers in kWh
    float getYieldDay() const;

    // minimum of all MPPT charge controllers' output voltages in V
    float getOutputVoltage() const;

    std::optional<VeDirectMpptController::data_t> getData(size_t idx = 0) const;

    bool isDataValid() const;

private:
    void loop();

    Task _loopTask;
    mutable std::mutex _mutex;
    std::unique_ptr<SolarChargerProvider> _upProvider = nullptr;
};

extern SolarChargerClass SolarCharger;
