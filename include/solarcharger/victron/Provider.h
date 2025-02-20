// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <mutex>
#include <memory>
#include <TaskSchedulerDeclarations.h>
#include <solarcharger/Provider.h>
#include <solarcharger/victron/Stats.h>
#include <VeDirectMpptController.h>

namespace SolarChargers::Victron {

class Provider : public ::SolarChargers::Provider {
public:
    Provider() = default;
    ~Provider() = default;

    bool init(bool verboseLogging) final;
    void deinit() final;
    void loop() final;
    void setChargeLimit( float limit, float act_charge_current) final;
    std::shared_ptr<::SolarChargers::Stats> getStats() const final { return _stats; }

private:
    Provider(Provider const& other) = delete;
    Provider(Provider&& other) = delete;
    Provider& operator=(Provider const& other) = delete;
    Provider& operator=(Provider&& other) = delete;

    mutable std::mutex _mutex;
    using controller_t = std::unique_ptr<VeDirectMpptController>;
    std::vector<controller_t> _controllers;
    std::vector<String> _serialPortOwners;
    std::shared_ptr<Stats> _stats = std::make_shared<Stats>();
    float _chargeLimit { 0.0f };
    float _chargeCurrent { 0.0f };

    bool initController(int8_t rx, int8_t tx, bool logging, uint8_t instance);
};

} // namespace SolarChargers::Victron
