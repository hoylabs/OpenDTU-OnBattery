// SPDX-License-Identifier: GPL-2.0-or-later
#include <Arduino.h>
#include <Configuration.h>
#include <MqttSettings.h>
#include <gridcharger/HTTP/Stats.h>

namespace GridChargers::HTTP {

uint32_t Stats::getLastUpdate() const
{
    return _dataPoints.getLastUpdate();
}

std::optional<float> Stats::getInputPower() const
{
   return _dataPoints.get<DataPointLabel::AcPower>();
}

void Stats::updateFrom(DataPointContainer const& dataPoints) const
{
    _dataPoints.updateFrom(dataPoints);
}

void Stats::getLiveViewData(JsonVariant& root) const
{
    root["vendorName"] = "HTTP";
    root["productName"] = "SmartSocket";
    root["provider"] = GridChargerProviderType::HTTP;

    const auto dataAge = millis() - getLastUpdate();
    root["dataAge"] = dataAge;
    root["reachable"] = dataAge < 10000;

    addValueInSection<DataPointLabel::AcPower>(root, "input", "power", 1);

}

}; // namespace GridChargers::HTTP
