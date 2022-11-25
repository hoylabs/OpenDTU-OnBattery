// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Configuration.h"
#include <espMqttClient.h>
#include <Arduino.h>
#include <Hoymiles.h>
#include <memory>

#define TOPIC_CURRENT_POWER_CONSUMPTION_1 "shellies/shellyem3/emeter/0/power"
#define TOPIC_CURRENT_POWER_CONSUMPTION_2 "shellies/shellyem3/emeter/1/power"
#define TOPIC_CURRENT_POWER_CONSUMPTION_3 "shellies/shellyem3/emeter/2/power"

class ZeroExportPowerLimiterClass {
public:
    void init();
    void loop();
    void onMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total);

private:
    uint32_t _lastLimitSet;
    uint32_t lastPowerMeterUpdate;
    uint16_t currentPowerLimit;

    float powerMeter1Power;
    float powerMeter2Power;
    float powerMeter3Power;
};

extern ZeroExportPowerLimiterClass ZeroExportPowerLimiter;
