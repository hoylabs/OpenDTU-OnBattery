// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>
#include <driver/twai.h>
#include <battery/CanReceiver.h>
#include <battery/pytes/Stats.h>
#include <battery/pytes/HassIntegration.h>

namespace Batteries::Pytes {

class Provider : public ::Batteries::CanReceiver {
public:
    bool init(bool verboseLogging) final;
    void onMessage(twai_message_t rx_message) final;

    std::shared_ptr<::Batteries::Stats> getStats() const final { return _stats; }
    ::Batteries::HassIntegration const& getHassIntegration() const final { return _hassIntegration; }

private:
    std::shared_ptr<Stats> _stats =
        std::make_shared<Stats>();
    HassIntegration _hassIntegration;
};

} // namespace Batteries::Pytes
