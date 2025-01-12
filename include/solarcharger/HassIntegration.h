// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <Arduino.h>

namespace SolarChargers {

class HassIntegration {
protected:
    void publish(const String& subtopic, const String& payload) const;
};

} // namespace SolarChargers
