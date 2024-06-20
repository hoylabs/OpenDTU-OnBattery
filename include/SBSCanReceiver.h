// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Configuration.h"
#include "Battery.h"
#include <espMqttClient.h>
#include <driver/twai.h>
#include <Arduino.h>
#include <memory>

class SBSCanReceiver : public BatteryProvider {
public:
    bool init(bool verboseLogging) final;
    void deinit() final;
    void loop() final;
    std::shared_ptr<BatteryStats> getStats() const final { return _stats; }

private:
    uint8_t readUnsignedInt8(uint8_t *data);
    uint16_t readUnsignedInt16(uint8_t *data);
    int32_t readSignedInt24(uint8_t *data);
    bool getBit(uint8_t value, uint8_t bit);
    void dummyData();
    bool _verboseLogging = true;
    std::shared_ptr<SBSBatteryStats> _stats =
        std::make_shared<SBSBatteryStats>();
};
