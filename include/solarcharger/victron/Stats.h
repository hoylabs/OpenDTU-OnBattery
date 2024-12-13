// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <solarcharger/Stats.h>
#include <VeDirectMpptController.h>
#include <map>

namespace SolarChargers::Victron {

class Stats : public ::SolarChargers::Stats {
public:
    void mqttPublish() const final;

private:
    mutable std::map<std::string, VeDirectMpptController::data_t> _kvFrames;

    // point of time in millis() when updated values will be published
    mutable uint32_t _nextPublishUpdatesOnly = 0;

    // point of time in millis() when all values will be published
    mutable uint32_t _nextPublishFull = 1;

    mutable bool _PublishFull;

    void publish_mppt_data(const VeDirectMpptController::data_t &mpptData,
                           const VeDirectMpptController::data_t &frame) const;
};

} // namespace SolarChargers::Victron
