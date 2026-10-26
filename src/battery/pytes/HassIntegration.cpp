// SPDX-License-Identifier: GPL-2.0-or-later

#include <battery/pytes/HassIntegration.h>

namespace Batteries::Pytes {

HassIntegration::HassIntegration(std::shared_ptr<Stats> spStats)
    : ::Batteries::HassIntegration(spStats)
    , _spPytesStats(spStats) { }

void HassIntegration::publishSensors() const
{
    ::Batteries::HassIntegration::publishSensors();

    publishSensor("Charge voltage (BMS)", NULL, "settings/chargeVoltage", "voltage", "measurement", "V");
    publishSensor("Charge current limit", NULL, "settings/chargeCurrentLimitation", "current", "measurement", "A");
    publishSensor("Discharge current limit", NULL, "settings/dischargeCurrentLimitation", "current", "measurement", "A");
    publishSensor("Discharge voltage limit", NULL, "settings/dischargeVoltageLimitation", "voltage", "measurement", "V");

    publishSensor("State of Health (SOH)", "mdi:heart-plus", "stateOfHealth", NULL, "measurement", "%");
    publishSensor("Temperature", "mdi:thermometer", "temperature", "temperature", "measurement", "°C");
    publishSensor("Charge Cycles", "mdi:counter", "chargeCycles");

    publishSensor("Charged Energy", NULL, "chargedEnergy", "energy", "total_increasing", "kWh");
    publishSensor("Discharged Energy", NULL, "dischargedEnergy", "energy", "total_increasing", "kWh");

    publishSensor("Total Capacity", NULL, "capacity");
    publishSensor("Available Capacity", NULL, "availableCapacity");

    publishSensor("Cell Min Voltage", NULL, "CellMinMilliVolt", "voltage", "measurement", "mV");
    publishSensor("Cell Max Voltage", NULL, "CellMaxMilliVolt", "voltage", "measurement", "mV");
    publishSensor("Cell Voltage Diff", "mdi:battery-alert", "CellDiffMilliVolt", "voltage", "measurement", "mV");
    publishSensor("Cell Min Temperature", NULL, "CellMinTemperature", "temperature", "measurement", "°C");
    publishSensor("Cell Max Temperature", NULL, "CellMaxTemperature", "temperature", "measurement", "°C");

    publishSensor("Cell Min Voltage Label", NULL, "CellMinVoltageName");
    publishSensor("Cell Max Voltage Label", NULL, "CellMaxVoltageName");
    publishSensor("Cell Min Temperature Label", NULL, "CellMinTemperatureName");
    publishSensor("Cell Max Temperature Label", NULL, "CellMaxTemperatureName");

    publishSensor("Modules Online", "mdi:counter", "modulesOnline");
    publishSensor("Modules Offline", "mdi:counter", "modulesOffline");
    publishSensor("Modules Blocking Charge", "mdi:counter", "modulesBlockingCharge");
    publishSensor("Modules Blocking Discharge", "mdi:counter", "modulesBlockingDischarge");
    publishSensor("Modules Heating", "mdi:heating-coil", "modulesHeating");

    publishBinarySensor("Alarm Discharge current", "mdi:alert", "alarm/overCurrentDischarge", "1", "0");
    publishBinarySensor("Alarm High charge current", "mdi:alert", "alarm/overCurrentCharge", "1", "0");
    publishBinarySensor("Alarm Voltage low", "mdi:alert", "alarm/underVoltage", "1", "0");
    publishBinarySensor("Alarm Voltage high", "mdi:alert", "alarm/overVoltage", "1", "0");
    publishBinarySensor("Alarm Temperature low", "mdi:thermometer-low", "alarm/underTemperature", "1", "0");
    publishBinarySensor("Alarm Temperature high", "mdi:thermometer-high", "alarm/overTemperature", "1", "0");
    publishBinarySensor("Alarm Temperature low (charge)", "mdi:thermometer-low", "alarm/underTemperatureCharge", "1", "0");
    publishBinarySensor("Alarm Temperature high (charge)", "mdi:thermometer-high", "alarm/overTemperatureCharge", "1", "0");
    publishBinarySensor("Alarm BMS internal", "mdi:alert", "alarm/bmsInternal", "1", "0");
    publishBinarySensor("Alarm Cell Imbalance", "mdi:alert-outline", "alarm/cellImbalance", "1", "0");

    publishBinarySensor("Warning Discharge current", "mdi:alert-outline", "warning/highCurrentDischarge", "1", "0");
    publishBinarySensor("Warning High charge current", "mdi:alert-outline", "warning/highCurrentCharge", "1", "0");
    publishBinarySensor("Warning Voltage low", "mdi:alert-outline", "warning/lowVoltage", "1", "0");
    publishBinarySensor("Warning Voltage high", "mdi:alert-outline", "warning/highVoltage", "1", "0");
    publishBinarySensor("Warning Temperature low", "mdi:thermometer-low", "warning/lowTemperature", "1", "0");
    publishBinarySensor("Warning Temperature high", "mdi:thermometer-high", "warning/highTemperature", "1", "0");
    publishBinarySensor("Warning Temperature low (charge)", "mdi:thermometer-low", "warning/lowTemperatureCharge", "1", "0");
    publishBinarySensor("Warning Temperature high (charge)", "mdi:thermometer-high", "warning/highTemperatureCharge", "1", "0");
    publishBinarySensor("Warning BMS internal", "mdi:alert-outline", "warning/bmsInternal", "1", "0");
    publishBinarySensor("Warning Cell Imbalance", "mdi:alert-outline", "warning/cellImbalance", "1", "0");

    publishBinarySensor("Balancing Active", "mdi:scale-balance", "balancingActive", "1", "0");
    publishBinarySensor("Charge immediately", "mdi:alert", "charging/chargeImmediately", "1", "0");

    // modules follow one per second, see publishDeferredSensors()
    _pendingModules = _spPytesStats->getModuleIdentities();
    _lastModulePublish = 0;
}

// Every module is its own HASS device (below the battery device). The Pytes
// CAN protocol has no module serials, the modules are keyed by their number,
// i.e., their position on the internal bus of the battery: after re-wiring,
// a device may stand for a different module. Per-cell sensors are disabled
// by default, they add up quickly (16 modules x 16 cells).
void HassIntegration::publishDeferredSensors() const
{
    if (_pendingModules.empty() || millis() - _lastModulePublish < 1000) { return; }
    _lastModulePublish = millis();

    auto const id = _pendingModules.back();
    _pendingModules.pop_back();

    String const number(id.number);
    auto const& manufacturer = _spPytesStats->getManufacturer();
    SubDevice const device {
        createBatteryId() + "_module_" + number,
        (manufacturer.has_value() ? *manufacturer : String("Battery")) + " Module " + number,
        "Module",
        id.fwVersion,
    };

    String const prefix = "module/" + number + "/";
    auto sensor = [&](String const& caption, char const* icon, String const& topic,
                      char const* deviceClass, char const* stateClass, char const* unit,
                      bool enabled = true, int8_t precision = -1) {
        publishSensor(caption.c_str(), icon, (prefix + topic).c_str(),
            deviceClass, stateClass, unit, enabled, &device, precision);
    };
    auto binarySensor = [&](String const& caption, char const* icon, String const& topic) {
        publishBinarySensor(caption.c_str(), icon, (prefix + topic).c_str(), "1", "0", true, &device);
    };

    // same captions as the battery level values and the Pytes RS485 modules
    sensor("Cell Min Voltage", nullptr, "CellMinMilliVolt", "voltage", "measurement", "mV");
    sensor("Cell Max Voltage", nullptr, "CellMaxMilliVolt", "voltage", "measurement", "mV");
    sensor("Cell Voltage Diff", "mdi:battery-alert", "CellDiffMilliVolt", "voltage", "measurement", "mV");
    sensor("Cell Min Temperature", nullptr, "CellMinTemperature", "temperature", "measurement", "°C");
    sensor("Cell Max Temperature", nullptr, "CellMaxTemperature", "temperature", "measurement", "°C");
    sensor("Temperature", "mdi:thermometer", "temperature", "temperature", "measurement", "°C");
    binarySensor("Heating", "mdi:heating-coil", "heating");
    // blocking comes from 0x40C (32-bit), so it covers every module
    binarySensor("Charging blocked", "mdi:battery-off", "blockingCharge");
    binarySensor("Discharging blocked", "mdi:battery-off", "blockingDischarge");

    for (uint8_t c = 1; c <= id.cellCount; ++c) {
        // explicitly String: "auto" would deduce Arduino's StringSumHelper,
        // whose operator+ appends in place and makes the captions pile up
        String const cell = "Cell " + String(c) + " ";
        String const topic = "cell/" + String(c) + "/";
        sensor(cell + "Voltage", nullptr, topic + "voltage", "voltage", "measurement", "V", false, 3);
        sensor(cell + "Temperature", nullptr, topic + "temperature", "temperature", "measurement", "°C", false);
    }
}

} // namespace Batteries::Pytes
