// SPDX-License-Identifier: GPL-2.0-or-later
#include "BatteryStats.h"
#include "MqttSettings.h"
#include "JkBmsDataPoints.h"

void BatteryStats::getLiveViewData(JsonVariant& root) const
{
    root[F("manufacturer")] = _manufacturer;
    root[F("data_age")] = getAgeSeconds();
    root[F("stateOfCharge")]["v"] = _SoC;
    root[F("stateOfCharge")]["u"] = "%";
    root[F("stateOfCharge")]["d"] = 0;
}

void PylontechBatteryStats::getLiveViewData(JsonVariant& root) const
{
    BatteryStats::getLiveViewData(root);

    root[F("chargeVoltage")]["v"] = _chargeVoltage;
    root[F("chargeVoltage")]["u"] = "V";
    root[F("chargeCurrentLimitation")]["v"] = _chargeCurrentLimitation;
    root[F("chargeCurrentLimitation")]["u"] = "A";
    root[F("dischargeCurrentLimitation")]["v"] = _dischargeCurrentLimitation;
    root[F("dischargeCurrentLimitation")]["u"] = "A";
    root[F("stateOfHealth")]["v"] = _stateOfHealth;
    root[F("stateOfHealth")]["u"] = "%";
    root[F("voltage")]["v"] = _voltage;
    root[F("voltage")]["u"] = "V";
    root[F("current")]["v"] = _current;
    root[F("current")]["u"] = "A";
    root[F("temperature")]["v"] = _temperature;
    root[F("temperature")]["u"] = "°C";

    // Alarms
    root["alarms"][F("dischargeCurrent")] = _alarmOverCurrentDischarge;
    root["alarms"][F("chargeCurrent")] = _alarmOverCurrentCharge;
    root["alarms"][F("lowTemperature")] = _alarmUnderTemperature;
    root["alarms"][F("highTemperature")] = _alarmOverTemperature;
    root["alarms"][F("lowVoltage")] = _alarmUnderVoltage;
    root["alarms"][F("highVoltage")] = _alarmOverVoltage;
    root["alarms"][F("bmsInternal")] = _alarmBmsInternal;

    // Warnings
    root["warnings"][F("dischargeCurrent")] = _warningHighCurrentDischarge;
    root["warnings"][F("chargeCurrent")] = _warningHighCurrentCharge;
    root["warnings"][F("lowTemperature")] = _warningLowTemperature;
    root["warnings"][F("highTemperature")] = _warningHighTemperature;
    root["warnings"][F("lowVoltage")] = _warningLowVoltage;
    root["warnings"][F("highVoltage")] = _warningHighVoltage;
    root["warnings"][F("bmsInternal")] = _warningBmsInternal;

    // Misc
    root[F("chargeEnabled")] = _chargeEnabled;
    root[F("dischargeEnabled")] = _dischargeEnabled;
    root[F("chargeImmediately")] = _chargeImmediately;
}

void JkBmsBatteryStats::getLiveViewData(JsonVariant& root) const
{
    using Label = JkBms::DataPointLabel;

    auto oVoltage = _dataPoints.get<Label::BatteryVoltageMilliVolt>();
    if (oVoltage.has_value()) {
        root[F("voltage")]["v"] = static_cast<float>(*oVoltage) / 1000;
        root[F("voltage")]["u"] = "V";
        root[F("voltage")]["d"] = 2;
    }

    auto oCurrent = _dataPoints.get<Label::BatteryCurrentMilliAmps>();
    if (oCurrent.has_value()) {
        root[F("current")]["v"] = static_cast<float>(*oCurrent) / 1000;
        root[F("current")]["u"] = "A";
        root[F("current")]["d"] = 2;
    }

    auto oTemperature = _dataPoints.get<Label::BatteryTempOneCelsius>();
    if (oTemperature.has_value()) {
        root[F("temperature")]["v"] = *oTemperature;
        root[F("temperature")]["u"] = "°C";
        root[F("temperature")]["d"] = 0;
    }
}

void BatteryStats::mqttPublish() const
{
    MqttSettings.publish(F("battery/manufacturer"), _manufacturer);
    MqttSettings.publish(F("battery/dataAge"), String(getAgeSeconds()));
    MqttSettings.publish(F("battery/stateOfCharge"), String(_SoC));
}

void PylontechBatteryStats::mqttPublish() const
{
    BatteryStats::mqttPublish();

    MqttSettings.publish(F("battery/settings/chargeVoltage"), String(_chargeVoltage));
    MqttSettings.publish(F("battery/settings/chargeCurrentLimitation"), String(_chargeCurrentLimitation));
    MqttSettings.publish(F("battery/settings/dischargeCurrentLimitation"), String(_dischargeCurrentLimitation));
    MqttSettings.publish(F("battery/stateOfHealth"), String(_stateOfHealth));
    MqttSettings.publish(F("battery/voltage"), String(_voltage));
    MqttSettings.publish(F("battery/current"), String(_current));
    MqttSettings.publish(F("battery/temperature"), String(_temperature));
    MqttSettings.publish(F("battery/alarm/overCurrentDischarge"), String(_alarmOverCurrentDischarge));
    MqttSettings.publish(F("battery/alarm/overCurrentCharge"), String(_alarmOverCurrentCharge));
    MqttSettings.publish(F("battery/alarm/underTemperature"), String(_alarmUnderTemperature));
    MqttSettings.publish(F("battery/alarm/overTemperature"), String(_alarmOverTemperature));
    MqttSettings.publish(F("battery/alarm/underVoltage"), String(_alarmUnderVoltage));
    MqttSettings.publish(F("battery/alarm/overVoltage"), String(_alarmOverVoltage));
    MqttSettings.publish(F("battery/alarm/bmsInternal"), String(_alarmBmsInternal));
    MqttSettings.publish(F("battery/warning/highCurrentDischarge"), String(_warningHighCurrentDischarge));
    MqttSettings.publish(F("battery/warning/highCurrentCharge"), String(_warningHighCurrentCharge));
    MqttSettings.publish(F("battery/warning/lowTemperature"), String(_warningLowTemperature));
    MqttSettings.publish(F("battery/warning/highTemperature"), String(_warningHighTemperature));
    MqttSettings.publish(F("battery/warning/lowVoltage"), String(_warningLowVoltage));
    MqttSettings.publish(F("battery/warning/highVoltage"), String(_warningHighVoltage));
    MqttSettings.publish(F("battery/warning/bmsInternal"), String(_warningBmsInternal));
    MqttSettings.publish(F("battery/charging/chargeEnabled"), String(_chargeEnabled));
    MqttSettings.publish(F("battery/charging/dischargeEnabled"), String(_dischargeEnabled));
    MqttSettings.publish(F("battery/charging/chargeImmediately"), String(_chargeImmediately));
}

void JkBmsBatteryStats::mqttPublish() const
{
    BatteryStats::mqttPublish();
}

void JkBmsBatteryStats::updateFrom(JkBms::DataPointContainer const& dp)
{
    _dataPoints.updateFrom(dp);

    using Label = JkBms::DataPointLabel;

    _manufacturer = "JKBMS";
    auto oProductId = _dataPoints.get<Label::ProductId>();
    if (oProductId.has_value()) {
        _manufacturer = oProductId->c_str();
        auto pos = oProductId->rfind("JK");
        if (pos != std::string::npos) {
            _manufacturer = oProductId->substr(pos).c_str();
        }
    }

    auto oSoCValue = _dataPoints.get<Label::BatterySoCPercent>();
    if (oSoCValue.has_value()) {
        _SoC = *oSoCValue;
        auto oSoCDataPoint = _dataPoints.getDataPointFor<Label::BatterySoCPercent>();
        _lastUpdateSoC = oSoCDataPoint->getTimestamp();
    }

    _lastUpdate = millis();
}
