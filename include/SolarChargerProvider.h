// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "VeDirectMpptController.h"

class SolarChargerProvider {
public:
    enum class Type : unsigned {
        VEDIRECT = 0
    };

     // returns true if the provider is ready for use, false otherwise
    virtual bool init(bool verboseLogging) = 0;
    virtual void deinit() = 0;
    virtual void loop() = 0;

    // TODO(andreasboehm): below methods are taken from VictronMppt to start abstracting
    // solar chargers without breaking everything.
    virtual size_t controllerAmount() = 0;
    virtual uint32_t getDataAgeMillis() = 0;
    virtual uint32_t getDataAgeMillis(size_t idx) = 0;
    // total output of all MPPT charge controllers in Watts
    virtual int32_t getPowerOutputWatts() = 0;

    // total panel input power of all MPPT charge controllers in Watts
    virtual int32_t getPanelPowerWatts() = 0;

    // sum of total yield of all MPPT charge controllers in kWh
    virtual float getYieldTotal() = 0;

    // sum of today's yield of all MPPT charge controllers in kWh
    virtual float getYieldDay() = 0;

    // minimum of all MPPT charge controllers' output voltages in V
    virtual float getOutputVoltage() = 0;

    virtual std::optional<VeDirectMpptController::data_t> getData(size_t idx = 0) = 0;

    virtual bool isDataValid() = 0;
};
