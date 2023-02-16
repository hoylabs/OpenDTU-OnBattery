// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Configuration.h"
#include <espMqttClient.h>
#include <Arduino.h>
#include <Hoymiles.h>
#include <memory>
#include "SDM.h"

#ifndef SDM_RX_PIN
#define SDM_RX_PIN 13
#endif

#ifndef SDM_TX_PIN
#define SDM_TX_PIN 32
#endif

class PowerMeterClass {
public:
    void init();
    void loop();
    void onMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total);
    float getPowerTotal();

private:
    uint32_t _interval;
    uint32_t _lastPowerMeterUpdate;
    bool _initDone = false;

    float _powerMeter1Power;
    float _powerMeter2Power;
    float _powerMeter3Power;
    float _powerMeterTotalPower;

};

extern PowerMeterClass PowerMeter;
