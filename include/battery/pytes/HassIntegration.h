// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <battery/HassIntegration.h>
#include <battery/pytes/Stats.h>

namespace Batteries::Pytes {

class HassIntegration : public ::Batteries::HassIntegration {
public:
    explicit HassIntegration(std::shared_ptr<Stats> spStats);

    void publishSensors() const final;
    void publishDeferredSensors() const final;

private:
    std::shared_ptr<Stats> _spPytesStats;
    mutable std::vector<Stats::ModuleIdentity> _pendingModules;
    mutable uint32_t _lastModulePublish = 0;
};

} // namespace Batteries::Pytes
