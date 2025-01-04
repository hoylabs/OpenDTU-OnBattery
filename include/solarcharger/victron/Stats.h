// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <solarcharger/Stats.h>
#include <VeDirectMpptController.h>
#include <map>

namespace SolarChargers::Victron {

class Stats : public ::SolarChargers::Stats {
public:
    uint32_t getAgeMillis() const final;
    std::optional<int32_t> getOutputPowerWatts() const final;
    std::optional<float> getOutputVoltage() const final;
    int32_t getPanelPowerWatts() const final;
    float getYieldTotal() const final;
    float getYieldDay() const final;

    void getLiveViewData(JsonVariant& root, boolean fullUpdate, uint32_t lastPublish) const final;
    void mqttPublish() const final;

    void update(const String serial, const std::optional<VeDirectMpptController::data_t> mpptData, uint32_t lastUpdate) const;

private:
    // TODO(andreasboehm): _data and _lastUpdate in two different structures is not ideal and needs to change
    mutable std::map<String, std::optional<VeDirectMpptController::data_t>> _data;
    mutable std::map<String, uint32_t> _lastUpdate;

    mutable std::map<String, VeDirectMpptController::data_t> _previousData;

    // point of time in millis() when updated values will be published
    mutable uint32_t _nextPublishUpdatesOnly = 0;

    // point of time in millis() when all values will be published
    mutable uint32_t _nextPublishFull = 1;

    mutable bool _PublishFull;

    void populateJsonWithInstanceStats(const JsonObject &root, const VeDirectMpptController::data_t &mpptData) const;

    void publish_mppt_data(const VeDirectMpptController::data_t &mpptData, const VeDirectMpptController::data_t &frame) const;
};

} // namespace SolarChargers::Victron
