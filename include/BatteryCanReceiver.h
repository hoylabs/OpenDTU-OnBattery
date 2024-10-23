// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Battery.h"
#include <driver/twai.h>
#include <Arduino.h>
#include <espMqttClient.h>

class BatteryCanReceiver : public BatteryProvider {
public:
    bool init(bool verboseLogging, char const* providerName);
    void deinit() final;
    void loop() final;

    virtual void onMessage(twai_message_t rx_message) = 0;

protected:
    uint8_t readUnsignedInt8(uint8_t *data);
    uint16_t readUnsignedInt16(uint8_t *data);
    int16_t readSignedInt16(uint8_t *data);
    uint32_t readUnsignedInt32(uint8_t *data);
    int32_t readSignedInt24(uint8_t *data);
    float scaleValue(int32_t value, float factor);
    bool getBit(uint8_t value, uint8_t bit);

    bool _verboseLogging = true;

private:
    char const* _providerName = "Battery CAN";

    enum CanInterface {
      kTwai,
      kMqtt,
    } _canInterface;
    String _canTopic;

    void postMessage(twai_message_t&& rx_message);
    void onMqttMessageCAN(espMqttClientTypes::MessageProperties const& properties,
            char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total);
};
