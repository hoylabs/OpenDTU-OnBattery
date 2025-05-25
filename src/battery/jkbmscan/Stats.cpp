// SPDX-License-Identifier: GPL-2.0-or-later
#include <sstream>
#include <MqttSettings.h>
#include <battery/jkbmscan/Stats.h>
#include <MessageOutput.h>
#include <Configuration.h>

namespace Batteries::JkBmsCan {

void Stats::getLiveViewData(JsonVariant& root) const
{
    ::Batteries::Stats::getLiveViewData(root);
    auto const& config = Configuration.get();
    uint8_t i;

    // values go into the "Status" card of the web application
    addLiveViewValue(root, "chargeVoltage", _chargeVoltage, "V", 1);
    addLiveViewValue(root, "chargeCurrentLimitation", _chargeCurrentLimitation, "A", 1);
    addLiveViewValue(root, "dischargeVoltageLimitation", _dischargeVoltageLimitation, "V", 1);
    addLiveViewValue(root, "stateOfHealth", _stateOfHealth, "%", 0);
    addLiveViewValue(root, "temperature", _temperature, "°C", 1);
    addLiveViewValue(root, "modules", _moduleCount, "", 0);
    std::string cellno;
    for (i=0; i<config.Battery.JkBmsCan.number_of_cells; i++)
    {
        if (i<10)
        {
            cellno="";
            cellno.append("Cell_0").append(std::to_string(i)).append("_Voltage");

        }
        else
        {
            cellno="";
            cellno.append("Cell_").append(std::to_string(i)).append("_Voltage");
        }
        addLiveViewValue(root, cellno, _cellVoltage[i], "mV", 0);
    }

    addLiveViewValue(root, "Number_Of_Cells", (float) config.Battery.JkBmsCan.number_of_cells, "Cells", 0);
    addLiveViewValue(root, "Max_Cell_Voltage", _MaxCellVoltage, "mV", 0);
    addLiveViewValue(root, "Max_Cell_Voltage_Number", _MaxCellVoltageNumber, "Cell", 0);
    addLiveViewValue(root, "Min_Cell_Voltage", _MinCellVoltage, "mV", 0);
    addLiveViewValue(root, "Min_Cell_Voltage_Number", _MinCellVoltageNumber, "Cell", 0);

    addLiveViewValue(root, "Capacity_Remaining", _capacityRemaining, "Ah", 0);
    addLiveViewValue(root, "Full_Charge_Cap", _fullChargeCapacity, "Ah", 0);
    addLiveViewValue(root, "Cycle_Capacity", _cycleCapacity, "Ah", 0);
    addLiveViewValue(root, "Cycle_Count", _cycleCount, " ", 0);




    addLiveViewTextValue(root, "chargeEnabled", (_chargeEnabled?"yes":"no"));
    addLiveViewTextValue(root, "dischargeEnabled", (_dischargeEnabled?"yes":"no"));
    addLiveViewTextValue(root, "balanceEnabled", (_balanceEnabled?"yes":"no"));
    addLiveViewTextValue(root, "heaterEnabled", (_heaterEnabled?"yes":"no"));
    addLiveViewTextValue(root, "chargeRequest", (_chargeRequest?"yes":"no"));

    // alarms and warnings go into the "Issues" card of the web application
    addLiveViewWarning(root, "highCurrentDischarge", _warningHighCurrentDischarge);
    addLiveViewAlarm(root, "overCurrentDischarge", _alarmOverCurrentDischarge);

    addLiveViewWarning(root, "highCurrentCharge", _warningHighCurrentCharge);
    addLiveViewAlarm(root, "overCurrentCharge", _alarmOverCurrentCharge);

    addLiveViewWarning(root, "lowTemperature", _warningLowTemperature);
    addLiveViewAlarm(root, "underTemperature", _alarmUnderTemperature);

    addLiveViewWarning(root, "highTemperature", _warningHighTemperature);
    addLiveViewAlarm(root, "overTemperature", _alarmOverTemperature);

    addLiveViewWarning(root, "lowVoltage", _warningLowVoltage);
    addLiveViewAlarm(root, "underVoltage", _alarmUnderVoltage);

    addLiveViewWarning(root, "highVoltage", _warningHighVoltage);
    addLiveViewAlarm(root, "overVoltage", _alarmOverVoltage);

    addLiveViewWarning(root, "bmsInternal", _warningBmsInternal);
    addLiveViewAlarm(root, "bmsInternal", _alarmBmsInternal);
}

void Stats::mqttPublish() const
{
    int i;
    ::Batteries::Stats::mqttPublish();
    auto const& config = Configuration.get();

    MqttSettings.publish("battery/settings/chargeVoltage", String(_chargeVoltage));
    MqttSettings.publish("battery/settings/chargeCurrentLimitation", String(_chargeCurrentLimitation));
    MqttSettings.publish("battery/settings/dischargeVoltageLimitation", String(_dischargeVoltageLimitation));
    MqttSettings.publish("battery/stateOfHealth", String(_stateOfHealth));
    MqttSettings.publish("battery/temperature", String(_temperature));
    MqttSettings.publish("battery/alarm/overCurrentDischarge", String(_alarmOverCurrentDischarge));
    MqttSettings.publish("battery/alarm/overCurrentCharge", String(_alarmOverCurrentCharge));
    MqttSettings.publish("battery/alarm/underTemperature", String(_alarmUnderTemperature));
    MqttSettings.publish("battery/alarm/overTemperature", String(_alarmOverTemperature));
    MqttSettings.publish("battery/alarm/underVoltage", String(_alarmUnderVoltage));
    MqttSettings.publish("battery/alarm/overVoltage", String(_alarmOverVoltage));
    MqttSettings.publish("battery/alarm/bmsInternal", String(_alarmBmsInternal));
    MqttSettings.publish("battery/warning/highCurrentDischarge", String(_warningHighCurrentDischarge));
    MqttSettings.publish("battery/warning/highCurrentCharge", String(_warningHighCurrentCharge));
    MqttSettings.publish("battery/warning/lowTemperature", String(_warningLowTemperature));
    MqttSettings.publish("battery/warning/highTemperature", String(_warningHighTemperature));
    MqttSettings.publish("battery/warning/lowVoltage", String(_warningLowVoltage));
    MqttSettings.publish("battery/warning/highVoltage", String(_warningHighVoltage));
    MqttSettings.publish("battery/warning/bmsInternal", String(_warningBmsInternal));
    MqttSettings.publish("battery/charging/chargeEnabled", String(_chargeEnabled));
    MqttSettings.publish("battery/charging/dischargeEnabled", String(_dischargeEnabled));
    MqttSettings.publish("battery/charging/chargeRequest", String(_chargeRequest));
    MqttSettings.publish("battery/modulesTotal", String(config.Battery.JkBmsCan.number_of_cells));
    String cellno;
    //char str[4];
    String str;
    for (i=0; i<config.Battery.JkBmsCan.number_of_cells; i++)
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
        MqttSettings.publish(cellno, String(_cellVoltage[i]));
        if (1) {
                MessageOutput.printf("[JkBmsCan] %s: %f \r\n",
                        cellno.c_str(), _cellVoltage[i]);
        }
       
    }
   


}

} // namespace Batteries::JkBmsCan
