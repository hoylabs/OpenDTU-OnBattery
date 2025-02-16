// SPDX-License-Identifier: GPL-2.0-or-later
#include <battery/zendure/Constants.h>
#include <battery/zendure/HassIntegration.h>

namespace Batteries::Zendure {

void HassIntegration::publishSensors() const
{
    ::Batteries::HassIntegration::publishSensors();

    publishSensor("Voltage", "mdi:battery-charging", "voltage", "voltage", "measurement", "V");
    publishSensor("Current", "mdi:current-dc", "current", "current", "measurement", "A");

    publishSensor("Cell Min Voltage", NULL, "CellMinMilliVolt", "voltage", "measurement", "mV");
    publishSensor("Cell Average Voltage", NULL, "CellAvgMilliVolt", "voltage", "measurement", "mV");
    publishSensor("Cell Max Voltage", NULL, "CellMaxMilliVolt", "voltage", "measurement", "mV");
    publishSensor("Cell Voltage Diff", "mdi:battery-alert", "CellDiffMilliVolt", "voltage", "measurement", "mV");
    publishSensor("Cell Max Temperature", NULL, "CellMaxTemperature", "temperature", "measurement", "°C");
    publishSensor("Charge Power", "mdi:battery-charging", "chargePower", "power", "measurement", "W");
    publishSensor("Discharge Power", "mdi:battery-discharging", "dischargePower", "power", "measurement", "W");
    publishBinarySensor("Battery Heating", NULL, "heating", "1", "0");
    publishSensor("State", NULL, "state");
    publishSensor("Number of Batterie Packs", "mdi:counter", "numPacks");
    publishSensor("Efficiency", NULL, "efficiency", NULL, "measurement", "%");
    publishSensor("Last Full Charge", "mdi:timelapse", "lastFullCharge", NULL, NULL, "h");

    for (size_t i = 1 ; i <= ZENDURE_MAX_PACKS ; i++) {
        const auto id = String(i);
        const auto bat = "Pack#" + id + ": ";
        publishSensor(bat + "Cell Min Voltage", NULL, id + "/CellMinMilliVolt", "voltage", "measurement", "mV");
        publishSensor(bat + "Cell Average Voltage", NULL, id + "/CellAvgMilliVolt", "voltage", "measurement", "mV");
        publishSensor(bat + "Cell Max Voltage", NULL, id + "/CellMaxMilliVolt", "voltage", "measurement", "mV");
        publishSensor(bat + "Cell Voltage Diff", "mdi:battery-alert", id + "/CellDiffMilliVolt", "voltage", "measurement", "mV");
        publishSensor(bat + "Cell Max Temperature", NULL, id + "/CellMaxTemperature", "temperature", "measurement", "°C");
        publishSensor(bat + "Power", NULL, id + "/power", "power", "measurement", "W");
        publishSensor(bat + "Voltage", NULL, id + "/voltage", "voltage", "measurement", "V");
        publishSensor(bat + "Current", NULL, id + "/current", "current", "measurement", "A");
        publishSensor(bat + "State Of Charge", NULL, id + "/stateOfCharge", NULL, "measurement", "%");
        publishSensor(bat + "State Of Health", NULL, id + "/stateOfHealth", NULL, "measurement", "%");
        publishSensor(bat + "State", NULL, id + "/state");
    }

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
}

} // namespace Batteries::Zendure
