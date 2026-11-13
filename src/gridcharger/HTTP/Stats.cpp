// SPDX-License-Identifier: GPL-2.0-or-later
#include <Arduino.h>
#include <Configuration.h>
#include <MqttSettings.h>
#include <gridcharger/HTTP/Stats.h>

namespace GridChargers::HTTP {

void Stats::setChargers(std::vector<std::shared_ptr<Charger const>> chargers)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _chargers = std::move(chargers);
}

std::vector<std::shared_ptr<Charger const>> Stats::getChargers() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _chargers;
}

uint32_t Stats::getLastUpdate() const
{
    uint32_t now = millis();
    uint32_t minAge = std::numeric_limits<uint32_t>::max();
    uint32_t lastUpdate = 0;

    for (auto const& charger : getChargers()) {
        auto update = charger->getLastUpdate();
        if (update == 0) { continue; }
        if (now - update < minAge) {
            minAge = now - update;
            lastUpdate = update;
        }
    }

    return lastUpdate;
}

std::optional<float> Stats::getInputPower() const
{
    std::optional<float> total;

    for (auto const& charger : getChargers()) {
        auto oPower = charger->getAcPower();
        if (!oPower) { continue; }
        total = total.value_or(0.0f) + *oPower;
    }

    return total;
}

void Stats::getLiveViewData(JsonVariant& root) const
{
    root["vendorName"] = "HTTP";
    root["productName"] = "SmartSocket";
    root["provider"] = GridChargerProviderType::HTTP;

    auto const chargers = getChargers();

    // the header reflects the charger with the oldest data, such that a
    // single unreachable charger is noticed.
    uint32_t now = millis();
    uint32_t dataAge = 0;
    for (auto const& charger : chargers) {
        auto lastUpdate = charger->getLastUpdate();
        dataAge = std::max(dataAge, lastUpdate > 0 ? now - lastUpdate : now);
    }
    root["dataAge"] = dataAge;
    root["reachable"] = !chargers.empty() && dataAge < 10000;

    auto oTotal = getInputPower();
    if (oTotal) {
        addValueInSection(root, "input", "power", *oTotal, "W", 1);
    }

    for (auto const& charger : chargers) {
        auto const& section = charger->getName();
        addStringInSection(root, section, "state", charger->getPowerState() ? "on" : "off");
        auto oPower = charger->getAcPower();
        if (oPower) {
            addValueInSection(root, section, "power", *oPower, "W", 1);
        }
        addValueInSection(root, section, "nominalPower", charger->getNominalPower(), "W", 0);
        addStringInSection(root, section, "reachable", charger->isDataFresh() ? "yes" : "no");
    }
}

}; // namespace GridChargers::HTTP
