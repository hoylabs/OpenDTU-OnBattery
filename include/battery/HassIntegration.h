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

    // publish the discovery configs again, e.g. once data they depend on
    // (like module serial numbers) became known
    void republish() { _publishSensors = true; }

protected:
    // a separate HASS device below the battery (e.g. a module of a multi-module
    // battery), its entities are keyed by id instead of the battery device id
    struct SubDevice {
        String id;      // globally unique, e.g. derived from a serial number
        String name;
        String model;
        String swVersion;
    };

    void publish(const String& subtopic, const String& payload) const;
    void publishBinarySensor(const char* caption,
            const char* icon, const char* subTopic,
            const char* payload_on, const char* payload_off,
            const bool enabled = true,
            SubDevice const* subDevice = nullptr) const;
    void publishSensor(const char* caption, const char* icon,
            const char* subTopic, const char* deviceClass = nullptr,
            const char* stateClass = nullptr,
            const char* unitOfMeasurement = nullptr,
            const bool enabled = true,
            SubDevice const* subDevice = nullptr,
            int8_t displayPrecision = -1) const; // -1: HASS default (0 decimals for V!)
    void createDeviceInfo(JsonObject& object) const;
    void createSubDeviceInfo(JsonObject& object, SubDevice const& subDevice) const;

    virtual void publishSensors() const;

private:
    static String sanitizeUniqueId(const char* value);
    static String sanitizeNodeId(String const& value);

    String _serial = "0001"; // pseudo-serial, can be replaced in future with real serialnumber
    std::shared_ptr<Stats> _spStats = nullptr;

    bool _publishSensors = true;
};

} // namespace Batteries
