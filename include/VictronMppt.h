// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <mutex>
#include <memory>
#include <TaskSchedulerDeclarations.h>

#include "SolarChargerProvider.h"
#include "VeDirectMpptController.h"
#include "Configuration.h"

class VictronMppt : public SolarChargerProvider {
public:
    VictronMppt() = default;
    ~VictronMppt() = default;

    bool init(bool verboseLogging) final;
    void deinit() final;
    void loop() final;

    bool isDataValid() const final;

    // returns the data age of all controllers,
    // i.e, the youngest data's age is returned.
    uint32_t getDataAgeMillis() const final;
    uint32_t getDataAgeMillis(size_t idx) const final;

    size_t controllerAmount() const final { return _controllers.size(); }
    std::optional<VeDirectMpptController::data_t> getData(size_t idx = 0) const final;

    // total output of all MPPT charge controllers in Watts
    int32_t getOutputPowerWatts() const final;

    // total panel input power of all MPPT charge controllers in Watts
    int32_t getPanelPowerWatts() const final;

    // sum of total yield of all MPPT charge controllers in kWh
    float getYieldTotal() const final;

    // sum of today's yield of all MPPT charge controllers in kWh
    float getYieldDay() const final;

    // minimum of all MPPT charge controllers' output voltages in V
    float getOutputVoltage() const final;

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
    VictronMppt(VictronMppt const& other) = delete;
    VictronMppt(VictronMppt&& other) = delete;
    VictronMppt& operator=(VictronMppt const& other) = delete;
    VictronMppt& operator=(VictronMppt&& other) = delete;

    mutable std::mutex _mutex;
    using controller_t = std::unique_ptr<VeDirectMpptController>;
    std::vector<controller_t> _controllers;

    std::vector<String> _serialPortOwners;
    bool initController(int8_t rx, int8_t tx, bool logging,
        uint8_t instance);
};
