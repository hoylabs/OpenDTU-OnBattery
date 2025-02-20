// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>
#include <mutex>
#include <TaskSchedulerDeclarations.h>
#include <solarcharger/Provider.h>
#include <solarcharger/Stats.h>

namespace SolarChargers {

class Controller {
public:
    void init(Scheduler&);
    void updateSettings();

    std::shared_ptr<Stats const> getStats() const;

private:
    void loop();

    Task _loopTask;
    mutable std::mutex _mutex;
    std::unique_ptr<Provider> _upProvider = nullptr;
    bool _forcePublishSensors = false;
    float _actChargeLimit = 0.0f;
};

} // namespace SolarChargers

extern SolarChargers::Controller SolarCharger;
