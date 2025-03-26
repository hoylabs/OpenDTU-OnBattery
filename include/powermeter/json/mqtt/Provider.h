// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <Configuration.h>
#include <powermeter/Provider.h>
#include <espMqttClient.h>
#include <vector>
#include <mutex>
#include <array>

namespace PowerMeters::Json::Mqtt {

class Provider : public ::PowerMeters::Provider {
public:
    explicit Provider(PowerMeterMqttConfig const& cfg)
        : _cfg(cfg) { }

    ~Provider();

    bool init() final;
    void loop() final { }

private:
    using MsgProperties = espMqttClientTypes::MessageProperties;
    void onMessage(MsgProperties const& properties, char const* topic,
            uint8_t const* payload, size_t len, size_t index,
            size_t total, uint8_t const phaseIndex, PowerMeterMqttValue const* cfg);

    PowerMeterMqttConfig const _cfg;

    std::vector<String> _mqttSubscriptions;
};

} // namespace PowerMeters::Json::Mqtt
