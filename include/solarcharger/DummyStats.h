// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <solarcharger/Stats.h>

namespace SolarChargers {

class DummyStats : public Stats {
public:
    uint32_t getAgeMillis() const override { return 0; }
    std::optional<int32_t> getOutputPowerWatts() const override { return std::nullopt; }
    std::optional<float> getOutputVoltage() const override { return std::nullopt; }
    int32_t getPanelPowerWatts() const override { return 0; }
    float getYieldTotal() const override { return 0; }
    float getYieldDay() const override { return 0; }
    void getLiveViewData(JsonVariant& root, const boolean fullUpdate, const uint32_t lastPublish) const override {}
    void mqttPublish() const override {}
    void mqttPublishSensors(const boolean forcePublish) const override {}
};

} // namespace SolarChargers
