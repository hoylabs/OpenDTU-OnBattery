// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>
#include <mutex>
#include <TaskSchedulerDeclarations.h>
#include <battery/Provider.h>
#include <battery/Stats.h>

namespace Batteries {

class Controller {
public:
    void init(Scheduler&);
    void updateSettings();

    float getDischargeCurrentLimit();

    std::shared_ptr<Stats const> getStats() const;

private:
    void loop();

    Task _loopTask;
    mutable std::mutex _mutex;
    std::unique_ptr<Provider> _upProvider = nullptr;
};

} // namespace Batteries

extern Batteries::Controller Battery;
