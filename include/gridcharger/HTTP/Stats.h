// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>
#include <mutex>
#include <vector>
#include <gridcharger/Stats.h>
#include <gridcharger/HTTP/Charger.h>

namespace GridChargers::HTTP {

class Stats : public ::GridChargers::Stats {
friend class Provider;

public:
    // the most recent update of any charger
    uint32_t getLastUpdate() const final;

    // sum of the AC power of all chargers with recent data
    std::optional<float> getInputPower() const final;

    void getLiveViewData(JsonVariant& root) const final;

protected:
    void mqttPublish() const final {}

    void setChargers(std::vector<std::shared_ptr<Charger const>> chargers);

private:
    std::vector<std::shared_ptr<Charger const>> getChargers() const;

    mutable std::mutex _mutex;
    std::vector<std::shared_ptr<Charger const>> _chargers;
};

} // namespace GridChargers::HTTP
