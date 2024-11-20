// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <mutex>
#include <memory>

#include "VeDirectMpptController.h"
#include "SolarChargerProvider.h"
#include "Configuration.h"
#include <TaskSchedulerDeclarations.h>

class VictronMppt : public SolarChargerProvider {
public:
    bool init(bool verboseLogging) final;
    void deinit() final;
    void loop() final;

    bool isDataValid() final;

    // returns the data age of all controllers,
    // i.e, the youngest data's age is returned.
    uint32_t getDataAgeMillis() final;
    uint32_t getDataAgeMillis(size_t idx) final;

    size_t controllerAmount() final { return _controllers.size(); }
    std::optional<VeDirectMpptController::data_t> getData(size_t idx = 0) final;

    // total output of all MPPT charge controllers in Watts
    int32_t getPowerOutputWatts() final;

    // total panel input power of all MPPT charge controllers in Watts
    int32_t getPanelPowerWatts() final;

    // sum of total yield of all MPPT charge controllers in kWh
    float getYieldTotal() final;

    // sum of today's yield of all MPPT charge controllers in kWh
    float getYieldDay() final;

    // minimum of all MPPT charge controllers' output voltages in V
    float getOutputVoltage() final;

    // returns the state of operation from the first available controller
    std::optional<uint8_t> getStateOfOperation() const;

    // returns the requested value from the first available controller in mV
    enum class MPPTVoltage : uint8_t {
            ABSORPTION = 0,
            FLOAT = 1,
            BATTERY = 2
    };
    std::optional<float> getVoltage(MPPTVoltage kindOf) const;

private:
    mutable std::mutex _mutex;
    using controller_t = std::unique_ptr<VeDirectMpptController>;
    std::vector<controller_t> _controllers;

    std::vector<String> _serialPortOwners;
    bool initController(int8_t rx, int8_t tx, bool logging,
        uint8_t instance);
};
