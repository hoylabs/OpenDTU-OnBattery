// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <battery/Stats.h>
#include <memory>

namespace Batteries {

class HassIntegration {
public:
    explicit HassIntegration(std::shared_ptr<Stats> spStats);

    void hassLoop();

protected:
    void publish(const String& subtopic, const String& payload) const;
    void publishBinarySensor(const char* caption,
            const char* icon, const char* subTopic,
            const char* payload_on, const char* payload_off) const;
    void publishSensor(const char* caption, const char* icon,
            const char* subTopic, const char* deviceClass = nullptr,
            const char* stateClass = nullptr,
            const char* unitOfMeasurement = nullptr) const;
    void publishSensor(const String& caption, const char* icon,
            const String& subTopic, const char* deviceClass = nullptr,
            const char* stateClass = nullptr,
            const char* unitOfMeasurement = nullptr) const;
    void publishSensor(const String& caption, const char* icon,
            const char* subTopic, const char* deviceClass = nullptr,
            const char* stateClass = nullptr,
            const char* unitOfMeasurement = nullptr) const;
    void publishSensor(const char* caption, const char* icon,
            const String& subTopic, const char* deviceClass = nullptr,
            const char* stateClass = nullptr,
            const char* unitOfMeasurement = nullptr) const;
    void createDeviceInfo(JsonObject& object) const;

    virtual void publishSensors() const;

private:
    String _serial = "0001"; // pseudo-serial, can be replaced in future with real serialnumber
    std::shared_ptr<Stats> _spStats = nullptr;

    bool _publishSensors = true;
};

} // namespace Batteries
