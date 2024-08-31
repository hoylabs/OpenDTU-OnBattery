// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Battery.h"

class VictronSmartBatterySense : public BatteryProvider {
public:
    bool init(bool verboseLogging) final;
    void deinit() final;
    void loop() final;
    std::shared_ptr<BatteryStats> getStats() const final { return _stats; }

private:
    // static char constexpr _serialPortOwner[] = "SmartBatterySense";

    uint32_t _lastUpdate = 0;
    std::shared_ptr<VictronSmartBatterySenseStats> _stats =
        std::make_shared<VictronSmartBatterySenseStats>();
};
