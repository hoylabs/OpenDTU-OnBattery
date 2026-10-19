// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <battery/Stats.h>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace Batteries {

class HassIntegration {
public:
    explicit HassIntegration(std::shared_ptr<Stats> spStats);
    virtual ~HassIntegration();

    void hassLoop();

protected:
    void publish(const String& subtopic, const String& payload) const;
    void publishBinarySensor(const char* caption,
            const char* icon, const char* subTopic,
            const char* payload_on, const char* payload_off,
            const bool enabled = true) const;
    void publishSensor(const char* caption, const char* icon,
            const char* subTopic, const char* deviceClass = nullptr,
            const char* stateClass = nullptr,
            const char* unitOfMeasurement = nullptr,
            const bool enabled = true) const;
    void createDeviceInfo(JsonObject& object) const;

    virtual void publishSensors() const;

private:
    static String sanitizeUniqueId(const char* value);
    static String createBatteryId();
    static String legacyConfigTopic(String const& component, String const& sensorId);
    void startLegacyCleanup();
    void clearLegacyConfigs();
    void stopLegacyCleanup();

    std::shared_ptr<Stats> _spStats = nullptr;

    bool _publishSensors = true;

    // see startLegacyCleanup()
    bool _legacyCleanupOnly = false;
    std::optional<uint32_t> _oLegacyCleanupStart;
    std::mutex _legacyTopicsMutex;
    std::vector<String> _legacyTopics; // filled by the MQTT task
};

} // namespace Batteries
