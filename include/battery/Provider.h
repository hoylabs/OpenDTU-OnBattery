// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>

namespace Batteries {

class Stats;
class HassIntegration;

class Provider {
public:
    // returns true if the provider is ready for use, false otherwise
    virtual bool init(bool verboseLogging) = 0;
    virtual void deinit() = 0;
    virtual void loop() = 0;
    virtual std::shared_ptr<Stats> getStats() const = 0;
    virtual std::shared_ptr<HassIntegration> getHassIntegration() = 0;
};

} // namespace Batteries
