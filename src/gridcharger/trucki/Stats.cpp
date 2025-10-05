// SPDX-License-Identifier: GPL-2.0-or-later
#include <MqttSettings.h>
#include <gridcharger/trucki/Stats.h>

namespace GridChargers::Trucki {

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
    root["vendorName"] = "Trucki";
    root["productName"] = "T2HG/T2MG";
    root["showSettings"] = false;

    root["dataAge"] = millis() - getLastUpdate();
    root["reachable"] = root["dataAge"] < 10000;

    using Label = GridChargers::Trucki::DataPointLabel;

    auto oDcPower = _dataPoints.get<Label::DcPower>();
    auto oDcCurrent = _dataPoints.get<Label::DcCurrent>();
    root["producing"] = oDcPower.value_or(0) > 10 && oDcCurrent.value_or(0) > 0.1;

    addStringInSection<Label::ZEPC>(root, "device", "zepc", false);
    addStringInSection<Label::State>(root, "device", "state", false);
    addValueInSection<Label::Temperature>(root, "device", "temp", 1);
    addValueInSection<Label::Efficiency>(root, "device", "efficiency", 0);
    addValueInSection<Label::DayEnergy>(root, "device", "yieldDay", 2);
    addValueInSection<Label::TotalEnergy>(root, "device", "yieldTotal", 2);

    addValueInSection<Label::AcVoltage>(root, "input", "voltage", 1);
    addValueInSection<Label::AcPower>(root, "input", "power", 1);
    addValueInSection<Label::AcPowerSetpoint>(root, "input", "powerSetpoint", 1);
    addValueInSection<Label::MinAcPower>(root, "input", "minPower", 0);
    addValueInSection<Label::MaxAcPower>(root, "input", "maxPower", 0);

    addValueInSection<Label::DcVoltage>(root, "output", "voltage", 2);
    addValueInSection<Label::DcVoltageSetpoint>(root, "output", "voltageSetpoint", 2);
    addValueInSection<Label::DcPower>(root, "output", "power", 1);
    addValueInSection<Label::DcCurrent>(root, "output", "current", 2);

    addValueInSection<Label::DcVoltageOffline>(root, "settings", "offlineVoltage", 0);
    addValueInSection<Label::DcCurrentOffline>(root, "settings", "offlineCurrent", 0);
}

}; // namespace GridChargers::Trucki
