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
        || millis() - _lastUpdateOutputCurrent > 60 * 1000) {
        return std::nullopt;
    }

    return value;
}

}; // namespace SolarChargers::Mqtt
