// SPDX-License-Identifier: GPL-2.0-or-later
#include <MqttSettings.h>
#include <battery/victronsmartshunt/Stats.h>
#include <Utils.h>

namespace Batteries::VictronSmartShunt {

void Stats::updateFrom(VeDirectShuntController::data_t const& shuntData) {
    _lastUpdate = VeDirectShunt.getLastUpdate();
    ::Batteries::Stats::setVoltage(shuntData.batteryVoltage_V_mV / 1000.0, _lastUpdate);
    ::Batteries::Stats::setSoC(static_cast<float>(shuntData.SOC) / 10, 1/*precision*/, _lastUpdate);
    ::Batteries::Stats::setCurrent(static_cast<float>(shuntData.batteryCurrent_I_mA) / 1000, 2/*precision*/, _lastUpdate);
    _fwversion = shuntData.getFwVersionFormatted();

    _chargeCycles = shuntData.H4;
    _timeToGo = shuntData.TTG / 60;
    _chargedEnergy = static_cast<float>(shuntData.H18) / 100;
    _dischargedEnergy = static_cast<float>(shuntData.H17) / 100;
    setManufacturer(String("Victron ") + shuntData.getPidAsString().data());
    _temperature = shuntData.T;
    _tempPresent = shuntData.tempPresent;
    _midpointVoltage = static_cast<float>(shuntData.VM) / 1000;
    _midpointDeviation = static_cast<float>(shuntData.DM) / 10;
    _instantaneousPower = shuntData.P;
    _consumedAmpHours = static_cast<float>(shuntData.CE) / 1000;
    _lastFullCharge = shuntData.H9 / 60;
    // shuntData.AR is a bitfield, so we need to check each bit individually
    _alarmLowVoltage = shuntData.alarmReason_AR & 1;
    _alarmHighVoltage = shuntData.alarmReason_AR & 2;
    _alarmLowSOC = shuntData.alarmReason_AR & 4;
    _alarmLowTemperature = shuntData.alarmReason_AR & 32;
    _alarmHighTemperature = shuntData.alarmReason_AR & 64;
}

void Stats::getLiveViewData(JsonVariant& root) const {
    ::Batteries::Stats::getLiveViewData(root);

    // values go into the "Status" card of the web application
    addLiveViewValue(root, "chargeCycles", _chargeCycles, "", 0);
    addLiveViewValue(root, "chargedEnergy", _chargedEnergy, "kWh", 2);
    addLiveViewValue(root, "dischargedEnergy", _dischargedEnergy, "kWh", 2);
    addLiveViewValue(root, "instantaneousPower", _instantaneousPower, "W", 0);
    addLiveViewValue(root, "consumedAmpHours", _consumedAmpHours, "Ah", 3);
    addLiveViewValue(root, "midpointVoltage", _midpointVoltage, "V", 2);
    addLiveViewValue(root, "midpointDeviation", _midpointDeviation, "%", 1);
    addLiveViewValue(root, "lastFullCharge", _lastFullCharge, "min", 0);
    if (_tempPresent) {
        addLiveViewValue(root, "temperature", _temperature, "°C", 0);
    }

    addLiveViewAlarm(root, "lowVoltage", _alarmLowVoltage);
    addLiveViewAlarm(root, "highVoltage", _alarmHighVoltage);
    addLiveViewAlarm(root, "lowSOC", _alarmLowSOC);
    addLiveViewAlarm(root, "lowTemperature", _alarmLowTemperature);
    addLiveViewAlarm(root, "highTemperature", _alarmHighTemperature);
}

void Stats::mqttPublish() const {
    ::Batteries::Stats::mqttPublish();

    MqttSettings.publish("battery/chargeCycles", String(_chargeCycles));
    MqttSettings.publish("battery/chargedEnergy", String(_chargedEnergy));
    MqttSettings.publish("battery/dischargedEnergy", String(_dischargedEnergy));
    MqttSettings.publish("battery/instantaneousPower", String(_instantaneousPower));
    MqttSettings.publish("battery/consumedAmpHours", String(_consumedAmpHours));
    MqttSettings.publish("battery/lastFullCharge", String(_lastFullCharge));
    MqttSettings.publish("battery/midpointVoltage", String(_midpointVoltage));
    MqttSettings.publish("battery/midpointDeviation", String(_midpointDeviation));
}

void Stats::checkSoCFullEpoch(void) {
    static uint32_t lastUpdate = 0;

    if (lastUpdate == _lastUpdate) { return; } // fast exit from already processed values
    lastUpdate = _lastUpdate;
    time_t nowEpoch;

    if (isSoCValid() && (getSoCAgeSeconds() <= 30) && Utils::getEpoch(&nowEpoch, 5)) {

        // invalid value from shunt, we do not process
        if ((_lastFullCharge < 0) || (_lastFullCharge > (365 * 24 * 60))) { return; }

        // reverse calculation to get 'fully charged epoch' from the smart shunt duration value
        auto shuntEpoch = std::make_optional(nowEpoch - static_cast<time_t>(_lastFullCharge * 60));
        auto lastEpoch = getSoCFullEpoch();

        // We only update if the value is not in the future and we don't have epoch yet or we have a newer value
        if ((shuntEpoch.value() <= nowEpoch) && (!lastEpoch.has_value() || (shuntEpoch.value() > lastEpoch.value()))) {
            setSoCFullEpoch(shuntEpoch);
        }
    }
}

} // namespace Batteries::VictronSmartShunt
