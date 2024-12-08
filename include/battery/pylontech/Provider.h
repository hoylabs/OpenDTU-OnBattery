// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>
#include <driver/twai.h>
#include <battery/CanReceiver.h>
#include <battery/pylontech/Stats.h>

namespace Batteries::Pylontech {

class Provider : public ::Batteries::CanReceiver {
public:
    bool init(bool verboseLogging) final;
    void onMessage(twai_message_t rx_message) final;

    std::shared_ptr<::Batteries::Stats> getStats() const final { return _stats; }

private:
    void dummyData();

    std::shared_ptr<Stats> _stats =
        std::make_shared<Stats>();
};

} // namespace Batteries::Pylontech
