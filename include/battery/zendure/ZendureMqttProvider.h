// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <battery/zendure/Stats.h>
#include <battery/zendure/Provider.h>

namespace Batteries::Zendure {

class ZendureMqttProvider : public ::Batteries::Zendure::Provider {
public:
    ZendureMqttProvider();
    bool init() final;
    void deinit() final;

private:
    void shutdown() const final {;};
    void timesync() final { ; };
    void writeSettings() final {;};
    void processPackDataJson(JsonVariantConst& packDataJson, const String& serial, const uint64_t timestamp) final;

    void processPackData(std::optional<JsonArrayConst>& packData, std::string& logValue, const uint64_t timestamp);
    void processProperties(std::optional<JsonObjectConst>& props, const uint64_t timestamp);

    void onMqttMessageReport(espMqttClientTypes::MessageProperties const& properties, char const* topic, uint8_t const* payload, size_t len);

    void performConnect();
    void performDisconnect();
    void performReconnect();
    bool getConnected();
    void createMqttClientObject();
    void NetworkEvent(network_event event);

    void onMqttDisconnect(espMqttClientTypes::DisconnectReason reason);
    void onMqttConnect(const bool sessionPresent);
    void onMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, const size_t len, const size_t index, const size_t total);

    void setBypassMode(BatteryZendureConfig::BypassMode_t mode) const {;};
    void setTopics(const String& deviceType, const String& deviceId);

    MqttClient* _mqttClient = nullptr;
    Ticker _mqttReconnectTimer;
    std::map<String, std::vector<uint8_t>> _fragments;
    MqttSubscribeParser _mqttSubscribeParser;
    std::mutex _clientLock;
};

} // namespace Batteries::Zendure
