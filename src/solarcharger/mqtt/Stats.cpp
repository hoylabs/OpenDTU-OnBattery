// SPDX-License-Identifier: GPL-2.0-or-later
#include <solarcharger/mqtt/Stats.h>
#include <Configuration.h>

namespace SolarChargers::Mqtt {

std::optional<float> Stats::getOutputPowerWatts() const
{
    return getValueIfNotOutdated(_lastUpdateOutputPowerWatts, _outputPowerWatts);
}

std::optional<float> Stats::getOutputVoltage() const
{
    return getValueIfNotOutdated(_lastUpdateOutputVoltage, _outputVoltage);
}

std::optional<float> Stats::getOutputCurrent() const {
    return getValueIfNotOutdated(_lastUpdateOutputCurrent, _outputCurrent);
}

void Stats::setOutputVoltage(const float voltage) {
    _outputVoltage = voltage;
    _lastUpdateOutputVoltage = _lastUpdate =  millis();

    auto outputCurrent = getOutputCurrent();
    if (Configuration.get().SolarCharger.Mqtt.CalculateOutputPower
        && outputCurrent) {
        setOutputPowerWatts(voltage * *outputCurrent);
    }
}

void Stats::setOutputCurrent(const float current) {
    _outputCurrent = current;
    _lastUpdateOutputCurrent = _lastUpdate =  millis();

    auto outputVoltage = getOutputVoltage();
    if (Configuration.get().SolarCharger.Mqtt.CalculateOutputPower
        && outputVoltage) {
        setOutputPowerWatts(*outputVoltage * current);
    }
}

std::optional<float> Stats::getValueIfNotOutdated(const uint32_t lastUpdate, const float value) const {
    // never updated or older than 60 seconds
    if (lastUpdate == 0
        || millis() - lastUpdate > 60 * 1000) {
        return std::nullopt;
    }

    return value;
}

void Stats::getLiveViewData(JsonVariant& root, const boolean fullUpdate, const uint32_t lastPublish) const
{
    ::SolarChargers::Stats::getLiveViewData(root, fullUpdate, lastPublish);

    auto age = millis() - _lastUpdate;

    auto hasUpdate = _lastUpdate > 0 && age < millis() - lastPublish;
    if (!fullUpdate && !hasUpdate) { return; }

    const JsonObject instance = root["solarcharger"]["instances"]["MQTT"].to<JsonObject>();
    instance["data_age_ms"] = age;
    instance["hide_serial"] = true;
    instance["product_id"] = "MQTT"; // will be translated by the web app

    const JsonObject output = instance["values"]["output"].to<JsonObject>();

    if (Configuration.get().SolarCharger.Mqtt.CalculateOutputPower) {
        output["P"]["v"] = _outputPowerWatts;
        output["P"]["u"] = "W";
        output["P"]["d"] = 1;
        output["V"]["v"] = _outputVoltage;
        output["V"]["u"] = "V";
        output["V"]["d"] = 2;
        output["I"]["v"] = _outputCurrent;
        output["I"]["u"] = "A";
        output["I"]["d"] = 2;
    }
    else {
        output["Power"]["v"] = _outputPowerWatts;
        output["Power"]["u"] = "W";
        output["Power"]["d"] = 1;

        auto outputVoltage = getOutputVoltage();
        if (outputVoltage) {
            output["V"]["v"] = *outputVoltage;
            output["V"]["u"] = "V";
            output["V"]["d"] = 2;
        }
    }
}

}; // namespace SolarChargers::Mqtt
