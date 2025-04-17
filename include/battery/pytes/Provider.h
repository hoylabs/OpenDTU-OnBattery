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
    Provider();
    bool init(bool verboseLogging) final;
    void onMessage(twai_message_t rx_message) final;

    std::shared_ptr<::Batteries::Stats> getStats() const final { return _stats; }
    std::shared_ptr<::Batteries::HassIntegration> getHassIntegration() final { return _hassIntegration; }

private:
    std::shared_ptr<Stats> _stats;
    std::shared_ptr<HassIntegration> _hassIntegration;
};

} // namespace Batteries::Pytes
