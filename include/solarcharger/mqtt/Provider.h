// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <mutex>
#include <memory>
#include <TaskSchedulerDeclarations.h>
#include <solarcharger/Provider.h>
#include <solarcharger/mqtt/Stats.h>
#include <VeDirectMpptController.h>
#include <espMqttClient.h>

namespace SolarChargers::Mqtt {

class Provider : public ::SolarChargers::Provider {
public:
    Provider() = default;
    ~Provider() = default;

    bool init(bool verboseLogging) final;
    void deinit() final;
    void loop() final { return; } // this class is event-driven
    void setChargeLimit(float limit, float act_charge_current) final { return; } 
    std::shared_ptr<::SolarChargers::Stats> getStats() const final { return _stats; }

private:
    Provider(Provider const& other) = delete;
    Provider(Provider&& other) = delete;
    Provider& operator=(Provider const& other) = delete;
    Provider& operator=(Provider&& other) = delete;

    bool _verboseLogging = false;
    String _outputPowerTopic;
    String _outputVoltageTopic;
    String _outputCurrentTopic;
    std::vector<String> _subscribedTopics;
    std::shared_ptr<Stats> _stats = std::make_shared<Stats>();

    void onMqttMessageOutputPower(espMqttClientTypes::MessageProperties const& properties,
            char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total,
            char const* jsonPath) const;

    void onMqttMessageOutputVoltage(espMqttClientTypes::MessageProperties const& properties,
            char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total,
            char const* jsonPath) const;

    void onMqttMessageOutputCurrent(espMqttClientTypes::MessageProperties const& properties,
            char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total,
            char const* jsonPath) const;
};

} // namespace SolarChargers::Mqtt
