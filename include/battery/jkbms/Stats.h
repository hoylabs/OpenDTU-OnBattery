// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <battery/Stats.h>
#include <battery/jkbms/DataPoints.h>

namespace Batteries::JkBms {

class Stats : public ::Batteries::Stats {
public:
    void getLiveViewData(JsonVariant& root) const final {
        getJsonData(root, false);
    }

    void getInfoViewData(JsonVariant& root) const {
        getJsonData(root, true);
    }

    void mqttPublish() const final;

    uint32_t getMqttFullPublishIntervalMs() const final { return 60 * 1000; }
    std::optional<String> getHassDeviceName() const final;

    void updateFrom(DataPointContainer const& dp);

private:
    void getJsonData(JsonVariant& root, bool verbose) const;

    DataPointContainer _dataPoints;
    mutable uint32_t _lastMqttPublish = 0;
    mutable uint32_t _lastFullMqttPublish = 0;

    uint16_t _cellMinMilliVolt = 0;
    uint16_t _cellAvgMilliVolt = 0;
    uint16_t _cellMaxMilliVolt = 0;
    uint32_t _cellVoltageTimestamp = 0;
};

} // namespace Batteries::JkBms
