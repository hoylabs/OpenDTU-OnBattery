#pragma once

#include <memory>
#include <espMqttClient.h>
#include <battery/Provider.h>
#include <battery/mqtt/Stats.h>

namespace Batteries::Mqtt {

class Provider : public ::Batteries::Provider {
public:
    Provider() = default;

    bool init(bool verboseLogging) final;
    void deinit() final;
    void loop() final { return; } // this class is event-driven
    std::shared_ptr<::Batteries::Stats> getStats() const final { return _stats; }
    std::shared_ptr<HassIntegration> getHassIntegration() final { return nullptr; }

private:
    bool _verboseLogging = false;
    String _socTopic;
    String _voltageTopic;
    String _dischargeCurrentLimitTopic;
    std::shared_ptr<Stats> _stats = std::make_shared<Stats>();
    uint8_t _socPrecision = 0;

    void onMqttMessageSoC(espMqttClientTypes::MessageProperties const& properties,
            char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total,
            char const* jsonPath);
    void onMqttMessageVoltage(espMqttClientTypes::MessageProperties const& properties,
            char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total,
            char const* jsonPath);
    void onMqttMessageDischargeCurrentLimit(espMqttClientTypes::MessageProperties const& properties,
            char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total,
            char const* jsonPath);
    uint8_t calculatePrecision(float value);
};

} // namespace Batteries::Mqtt
