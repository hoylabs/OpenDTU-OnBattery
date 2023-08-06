// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Configuration.h"
#include <espMqttClient.h>
#include <driver/twai.h>
#include <Arduino.h>
#include <memory>

class PylontechBatteryStats;

class PylontechCanReceiverClass {
public:
    void init(int8_t rx, int8_t tx, std::shared_ptr<PylontechBatteryStats> stats);
    void deinit();
    void loop();

private:
    void parseCanPackets();
    uint16_t readUnsignedInt16(uint8_t *data);
    int16_t readSignedInt16(uint8_t *data);
    float scaleValue(int16_t value, float factor);
    bool getBit(uint8_t value, uint8_t bit);

    std::shared_ptr<PylontechBatteryStats> _stats;
};

extern PylontechCanReceiverClass PylontechCanReceiver;
