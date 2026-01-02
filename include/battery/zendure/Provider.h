// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>
#include <battery/Provider.h>
#include <battery/zendure/Stats.h>
#include <battery/zendure/HassIntegration.h>
#include <battery/zendure/Constants.h>
#include <espMqttClient.h>
#include <Configuration.h>

namespace Batteries::Zendure {

class Provider : public ::Batteries::Provider {
public:
    Provider();
    bool init();
    void deinit();
    void loop() final;
    std::shared_ptr<::Batteries::Stats> getStats() const final { return _stats; }
    std::shared_ptr<::Batteries::HassIntegration> getHassIntegration() final { return _hassIntegration; }

protected:
    void processPackData(std::optional<JsonArrayConst>& packData, std::string& logValue, const uint64_t timestamp);
    void processProperties(std::optional<JsonObjectConst>& props, const uint64_t timestamp);

    void calculatePackStats(const uint64_t timestamp);
    void calculateEfficiency();
    void setSoC(const float soc, const uint32_t timestamp = 0, const uint8_t precision = 2);
    bool alive() const { return _stats->getAgeSeconds() < ZENDURE_ALIVE_SECONDS; }

    void setChargeThroughState(const ChargeThroughState value, const bool publish = true);

    void publishProperty(const String& topic, const String& property, const String& value) const;
    template<typename... Arg>
    void publishProperties(const String& topic, Arg&&... args) const;

    void onMqttMessagePersistentSettings(espMqttClientTypes::MessageProperties const& properties,
            char const* topic, uint8_t const* payload, size_t len);

    virtual void timesync() = 0;
    virtual void shutdown() const = 0;
    virtual void writeSettings() = 0;
    virtual void processPackDataJson(JsonVariantConst& packDataJson, const String& serial, const uint64_t timestamp) = 0;
    virtual void setBypassMode(BatteryZendureConfig::BypassMode_t mode) const = 0;

    std::shared_ptr<Stats> _stats = std::make_shared<Stats>();
    std::shared_ptr<HassIntegration> _hassIntegration;

    uint32_t _messageCounter = 0;

    String _topicLog = String();
    String _topicReadReply = String();
    String _topicReport = String();
    String _topicRead = String();
    String _topicWrite = String();
    String _topicTimesync = String();
    String _topicTimesyncReply = String();
    String _topicPersistentSettings = String();

    String _payloadFullUpdate = String();

    bool _full_log_supported = false;

private:
    uint16_t setOutputLimit(uint16_t limit) const;
    uint16_t setInverterMax(uint16_t limit) const;
    void checkChargeThrough(uint32_t predictHours = 0U);
    uint16_t calcOutputLimit(uint16_t limit) const;
    void setTargetSoCs(const float soc_min, const float soc_max);

    void calculateFullChargeAge();
    void rescheduleSunCalc() { _nextSunCalc = 0; }
    void publishPersistentSettings(const char* subtopic, const String& payload);

    uint32_t _rateFullUpdateMs = 0;
    uint64_t _nextFullUpdate = 0;

    uint32_t _rateTimesyncMs = 0;
    uint64_t _nextTimesync = 0;

    uint32_t _rateSunCalcMs = 0;
    uint64_t _nextSunCalc = 0;
};

} // namespace Batteries::Zendure
