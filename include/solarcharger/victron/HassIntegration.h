// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <ArduinoJson.h>
#include <solarcharger/HassIntegration.h>
#include "VeDirectMpptController.h"

namespace SolarChargers::Victron {

class HassIntegration : public ::SolarChargers::HassIntegration {
public:
    void publishSensors(const VeDirectMpptController::data_t &mpptData) const;

private:
    void publishBinarySensor(const char *caption, const char *icon, const char *subTopic,
                             const char *payload_on, const char *payload_off,
                             const VeDirectMpptController::data_t &mpptData) const;

    void publishSensor(const char *caption, const char *icon, const char *subTopic,
                       const char *deviceClass, const char *stateClass,
                       const char *unitOfMeasurement,
                       const VeDirectMpptController::data_t &mpptData) const;

    void createDeviceInfo(JsonObject &object,
                          const VeDirectMpptController::data_t &mpptData) const;
};

} // namespace SolarChargers::Victron
