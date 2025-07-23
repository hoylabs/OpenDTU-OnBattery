// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <powermeter/DataPoints.h>
#include <memory>

namespace PowerMeters {

class HassIntegration {
public:
    void hassLoop();

private:
    void publishSensors();
    void publishSensor(const char* caption, const char* icon,
            const char* subTopic, const char* deviceClass = nullptr,
            const char* stateClass = nullptr,
            const char* unitOfMeasurement = nullptr,
            const bool enabled = true);
    void createDeviceInfo(JsonObject& object);
    void publish(const String& subtopic, const String& payload);
    
    static String sanitizeUniqueId(const char* value);

    String _serial = "0001"; // pseudo-serial, matches DTU unique ID
    bool _publishSensors = true;
};

} // namespace PowerMeters

extern PowerMeters::HassIntegration PowerMeterHassIntegration;