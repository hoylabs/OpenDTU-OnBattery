// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace SolarChargers {

class HassIntegration {
public:
    virtual void publishSensors() const;

protected:
    void publish(const String& subtopic, const String& payload) const;
};

} // namespace SolarChargers
