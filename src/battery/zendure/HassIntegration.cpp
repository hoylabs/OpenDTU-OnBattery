// SPDX-License-Identifier: GPL-2.0-or-later
#include <battery/zendure/Constants.h>
#include <battery/zendure/HassIntegration.h>

namespace Batteries::Zendure {

 HassIntegration::HassIntegration(std::shared_ptr<Stats> spStats)
    : ::Batteries::HassIntegration(spStats) { }

void HassIntegration::publishSensors() const
{
    ::Batteries::HassIntegration::publishSensors();

    publishSensor("Cell Min Voltage", NULL, "cellMinMilliVolt", "voltage", "measurement", "mV");
    publishSensor("Cell Average Voltage", NULL, "cellAvgMilliVolt", "voltage", "measurement", "mV");
    publishSensor("Cell Max Voltage", NULL, "cellMaxMilliVolt", "voltage", "measurement", "mV");
    publishSensor("Cell Voltage Diff", "mdi:battery-alert", "cellDiffMilliVolt", "voltage", "measurement", "mV");
    publishSensor("Cell Max Temperature", NULL, "cellMaxTemperature", "temperature", "measurement", "°C");
    publishSensor("Charge Power", "mdi:battery-charging", "chargePower", "power", "measurement", "W");
    publishSensor("Discharge Power", "mdi:battery-discharging", "dischargePower", "power", "measurement", "W");
    publishBinarySensor("Battery Heating", NULL, "heating", "1", "0");
    publishSensor("State", NULL, "state");
    publishSensor("Number of Batterie Packs", "mdi:counter", "numPacks");
    publishSensor("Efficiency", NULL, "efficiency", NULL, "measurement", "%");
    publishSensor("Last Full Charge", "mdi:timelapse", "lastFullCharge", NULL, NULL, "h");

    publishSensor("Solar Power MPPT 1", "mdi:solar-power", "solarPowerMppt1", "power", "measurement", "W");
    publishSensor("Solar Power MPPT 2", "mdi:solar-power", "solarPowerMppt2", "power", "measurement", "W");
    publishSensor("Total Output Power", NULL, "outputPower", "power", "measurement", "W");
    publishSensor("Total Input Power", NULL, "inputPower", "power", "measurement", "W");
    publishBinarySensor("Bypass State", NULL, "bypass", "1", "0");

    publishSensor("Output Power Limit", NULL, "settings/outputLimitPower", "power", "settings", "W");
    publishSensor("Input Power Limit", NULL, "settings/inputLimitPower", "power", "settings", "W");
    publishSensor("Minimum State of Charge", NULL, "settings/stateOfChargeMin", NULL, "settings", "%");
    publishSensor("Maximum State of Charge", NULL, "settings/stateOfChargeMax", NULL, "settings", "%");
    publishSensor("Bypass Mode", NULL, "settings/bypassMode", "settings");

    for (size_t i = 1 ; i <= ZENDURE_MAX_PACKS ; i++) {
        const auto id = String(i);
        const auto bat = String("Pack#" + id + ": ");
        publishSensor(bat + "Cell Min Voltage", NULL, id + "/cellMinMilliVolt", "voltage", "measurement", "mV", false);
        publishSensor(bat + "Cell Average Voltage", NULL, id + "/cellAvgMilliVolt", "voltage", "measurement", "mV", false);
        publishSensor(bat + "Cell Max Voltage", NULL, id + "/cellMaxMilliVolt", "voltage", "measurement", "mV", false);
        publishSensor(bat + "Cell Voltage Diff", "mdi:battery-alert", id + "/cellDiffMilliVolt", "voltage", "measurement", "mV", false);
        publishSensor(bat + "Cell Max Temperature", NULL, id + "/cellMaxTemperature", "temperature", "measurement", "°C", false);
        publishSensor(bat + "Power", NULL, id + "/power", "power", "measurement", "W", false);
        publishSensor(bat + "Voltage", NULL, id + "/voltage", "voltage", "measurement", "V", false);
        publishSensor(bat + "Current", NULL, id + "/current", "current", "measurement", "A", false);
        publishSensor(bat + "State Of Charge", NULL, id + "/stateOfCharge", NULL, "measurement", "%", false);
        publishSensor(bat + "State Of Health", NULL, id + "/stateOfHealth", NULL, "measurement", "%", false);
        publishSensor(bat + "State", NULL, id + "/state", NULL, NULL, NULL, false);
    }
}

} // namespace Batteries::Zendure
