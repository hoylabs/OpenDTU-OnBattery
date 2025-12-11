// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/*
 * Locking policy:
 * - Public getters having the prefix 'get' take a shared lock internally.
 * - Public mutating methods take an exclusive lock internally.
 * - Private getters having the prefix 'g' do not take a lock.
 * - Private methods are called with the appropriate lock held by the public methods.
 * - Fetching external data is done before acquiring _mutex. The only exception is data from the configuration or
 *   data used just for visualization.
 * - Flags marked as atomic may be read lock-free.
 */

#include <powermeter/Provider.h>
#include <TaskSchedulerDeclarations.h>
#include <memory>
#include <mutex>

namespace InverterMeters {

class Controller {
public:
    void init(Scheduler& scheduler);
    void updateSettings();

    // todo: consider removing getPowerTotal in favor of getPower with inverterID
    float getPowerTotal() const;
    uint32_t getLastUpdate() const;

    // returns true if the data is not older than 30 seconds
    bool isDataValid() const;

    // returns power for the given inverter ID if available
    std::optional<float> getPower(uint64_t inverterID) const;

    // returns the time of the last update for the given inverter serial number
    uint32_t getTime(uint64_t inverterSN) const;

    // returns the time of the measurement request in milliseconds
    uint32_t getRequestTime() const;

private:
    void loop();

    Task _loopTask;
    mutable std::mutex _mutex;
    std::unique_ptr<PowerMeters::Provider> _upProvider = nullptr;
};

} // namespace InverterMeters

extern InverterMeters::Controller InverterMeter;
