// SPDX-License-Identifier: GPL-2.0-or-later

#include <battery/jkbmscan/HassIntegration.h>
#include <Configuration.h>

namespace Batteries::JkBmsCan {

HassIntegration::HassIntegration(std::shared_ptr<Stats> spStats)
    : ::Batteries::HassIntegration(spStats) { }

void HassIntegration::publishSensors() const
{
    ::Batteries::HassIntegration::publishSensors();
    uint8_t i;
    auto const& config = Configuration.get();

    publishSensor("Temperature", NULL, "temperature", "temperature", "measurement", "°C");
    publishSensor("State of Health (SOH)", "mdi:heart-plus", "stateOfHealth", NULL, "measurement", "%");
    publishSensor("Charge voltage (BMS)", NULL, "settings/chargeVoltage", "voltage", "measurement", "V");
    publishSensor("Charge current limit", NULL, "settings/chargeCurrentLimitation", "current", "measurement", "A");
    publishSensor("Discharge voltage limit", NULL, "settings/dischargeVoltageLimitation", "voltage", "measurement", "V");
    publishSensor("Discharge current limit", NULL, "settings/dischargeCurrentLimitation", "current", "measurement", "A");
    publishSensor("Module Count", "mdi:counter", "modulesTotal");

    publishBinarySensor("Alarm Discharge current", "mdi:alert", "alarm/overCurrentDischarge", "1", "0");
    publishBinarySensor("Warning Discharge current", "mdi:alert-outline", "warning/highCurrentDischarge", "1", "0");

    publishBinarySensor("Alarm Temperature low", "mdi:thermometer-low", "alarm/underTemperature", "1", "0");
    publishBinarySensor("Warning Temperature low", "mdi:thermometer-low", "warning/lowTemperature", "1", "0");

    publishBinarySensor("Alarm Temperature high", "mdi:thermometer-high", "alarm/overTemperature", "1", "0");
    publishBinarySensor("Warning Temperature high", "mdi:thermometer-high", "warning/highTemperature", "1", "0");

    publishBinarySensor("Alarm Voltage low", "mdi:alert", "alarm/underVoltage", "1", "0");
    publishBinarySensor("Warning Voltage low", "mdi:alert-outline", "warning/lowVoltage", "1", "0");

    publishBinarySensor("Alarm Voltage high", "mdi:alert", "alarm/overVoltage", "1", "0");
    publishBinarySensor("Warning Voltage high", "mdi:alert-outline", "warning/highVoltage", "1", "0");

    publishBinarySensor("Alarm BMS internal", "mdi:alert", "alarm/bmsInternal", "1", "0");
    publishBinarySensor("Warning BMS internal", "mdi:alert-outline", "warning/bmsInternal", "1", "0");

    publishBinarySensor("Alarm High charge current", "mdi:alert", "alarm/overCurrentCharge", "1", "0");
    publishBinarySensor("Warning High charge current", "mdi:alert-outline", "warning/highCurrentCharge", "1", "0");

    publishBinarySensor("Charge enabled", "mdi:battery-arrow-up", "charging/chargeEnabled", "1", "0");
    publishBinarySensor("Discharge enabled", "mdi:battery-arrow-down", "charging/dischargeEnabled", "1", "0");
    publishBinarySensor("Charge immediately", "mdi:alert", "charging/chargeImmediately", "1", "0");

    String cellno;
    //char str[4];
    String str;
    for (i=0; i<config.Battery.JkBmsCan.NumberOfCells; i++)
    {
        str = String(i); //itoa(i, str, 10);
        if (i>99)
        {
            i=99;
        }
        if (i<10)
        {
            cellno="battery/Cell0"+str+"Voltage";
            //cellno.concat("battery/Cell0");
            //cellno.concat(str);
            //cellno.concat("Voltage");
        }
        else
        {
            cellno="battery/Cell0"+str+"Voltage";
            //cellno.concat("battery/Cell");
            //cellno.concat(str);
            //cellno.concat("Voltage");
        }
        publishSensor(cellno.c_str(), NULL, cellno.c_str(), "voltage", "measurement", "mV");
    }

    



}

} // namespace Batteries::JkBmsCan
