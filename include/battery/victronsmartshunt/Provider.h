// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>
#include <battery/Provider.h>
#include <battery/victronsmartshunt/Stats.h>
#include <battery/victronsmartshunt/HassIntegration.h>

namespace Batteries::VictronSmartShunt {

class Provider : public ::Batteries::Provider {
public:
    bool init(bool verboseLogging) final;
    void deinit() final;
    void loop() final;
    std::shared_ptr<::Batteries::Stats> getStats() const final { return _stats; }
    ::Batteries::HassIntegration const& getHassIntegration() const final { return _hassIntegration; }

private:
    static char constexpr _serialPortOwner[] = "SmartShunt";

    uint32_t _lastUpdate = 0;
    std::shared_ptr<Stats> _stats =
        std::make_shared<Stats>();
    HassIntegration _hassIntegration;
};

} // namespace Batteries::VictronSmartShunt
