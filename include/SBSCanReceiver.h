// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Configuration.h"
#include "Battery.h"
#include "BatteryCanReceiver.h"
#include <driver/twai.h>
#include <Arduino.h>

class SBSCanReceiver : public BatteryCanReceiver {
public:
    bool init(bool verboseLogging) final;
    void onMessage(twai_message_t rx_message) final;

    std::shared_ptr<BatteryStats> getStats() const final { return _stats; }

private:
    int32_t readSignedInt24(uint8_t *data);
    void dummyData();

    std::shared_ptr<SBSBatteryStats> _stats =
        std::make_shared<SBSBatteryStats>();
};
