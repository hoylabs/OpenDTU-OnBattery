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
    for (i=0; i<config.Battery.JkBmsCan.NumberOfCells; i++)
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

    addLiveViewValue(root, "Number_Of_Cells", (float) config.Battery.JkBmsCan.NumberOfCells, "Cells", 0);
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
    MqttSettings.publish("battery/modulesTotal", String(config.Battery.JkBmsCan.NumberOfCells));
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
        MqttSettings.publish(cellno, String(_cellVoltage[i]));
       
    }
   


}


void Stats::updateFromV2(uint8_t* rx, uint32_t now)
{
    _v2ErrorMask =
        rx[0] |
        (rx[1] << 8) |
        (rx[2] << 16);

    _lastV2Ts = now;
}

void Stats::updateFromV1(uint8_t* rx, uint32_t now)
{
    _v1SeverityMask =
        (uint64_t)rx[0] |
        ((uint64_t)rx[1] << 8) |
        ((uint64_t)rx[2] << 16) |
        ((uint64_t)rx[3] << 24);

    _lastV1Ts = now;
}

void Stats::evaluateErrors(uint32_t now)
{
    // Set an error timeout of 2 seconds.
    const uint32_t timeout = 2000;

    // Check whether an error was received within the last validity window.
    bool v2Valid = (now - _lastV2Ts) < timeout;
    bool v1Valid = (now - _lastV1Ts) < timeout;

    // Reset all errors.
    _alarmOverCurrentDischarge = false;
    _alarmOverCurrentCharge = false;
    _alarmUnderTemperature = false;
    _alarmOverTemperature = false;
    _alarmUnderVoltage = false;
    _alarmOverVoltage = false;
    _alarmBmsInternal = false;

    _warningHighCurrentDischarge = false;
    _warningHighCurrentCharge = false;
    _warningLowTemperature = false;
    _warningHighTemperature = false;
    _warningLowVoltage = false;
    _warningHighVoltage = false;
    _warningBmsInternal = false;

    if (v2Valid) {
        applyV2();
    }
    else if (v1Valid) {
        applyV1();
    }
}

void Stats::applyV2()
{
    uint32_t m = _v2ErrorMask;

    _alarmOverVoltage          = m & (1 << 4);
    _alarmUnderVoltage         = m & (1 << 11);
    _alarmOverCurrentCharge    = m & (1 << 6);
    _alarmOverCurrentDischarge = m & (1 << 13);
    _alarmOverTemperature      = m & (1 << 8);
    _alarmUnderTemperature     = m & (1 << 9);
    _alarmBmsInternal          = m & (1 << 10);
}

uint8_t Stats::getSeverity(uint8_t alarm)
{
    uint8_t shift = (alarm - 1) * 2;
    return (_v1SeverityMask >> shift) & 0x3;
}

void Stats::applyV1()
{
    auto map = [&](uint8_t sev, bool& alarm, bool& warn)
    {
        if (sev == 1) alarm = true;
        else if (sev == 2 || sev == 3) warn = true;
    };

    map(getSeverity(1), _alarmOverVoltage, _warningHighVoltage);
    map(getSeverity(2), _alarmUnderVoltage, _warningLowVoltage);
    map(getSeverity(6), _alarmOverCurrentDischarge, _warningHighCurrentDischarge);
    map(getSeverity(7), _alarmOverCurrentCharge, _warningHighCurrentCharge);
    map(getSeverity(8), _alarmOverTemperature, _warningHighTemperature);
    map(getSeverity(9), _alarmUnderTemperature, _warningLowTemperature);
    map(getSeverity(15), _alarmBmsInternal, _warningBmsInternal);
}


} // namespace Batteries::JkBmsCan
