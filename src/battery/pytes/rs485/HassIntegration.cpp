// SPDX-License-Identifier: GPL-2.0-or-later
#include <battery/pytes/rs485/HassIntegration.h>

namespace Batteries::Pytes::Rs485 {

HassIntegration::HassIntegration(std::shared_ptr<Stats> spStats)
    : ::Batteries::HassIntegration(spStats)
    , _spRs485Stats(spStats) { }

void HassIntegration::publishSensors() const
{
    ::Batteries::HassIntegration::publishSensors();

    publishSensor("Charge voltage (BMS)", nullptr, "settings/chargeVoltage", "voltage", "measurement", "V", true, nullptr, 2);
    publishSensor("Charge current limit", nullptr, "settings/chargeCurrentLimitation", "current", "measurement", "A");
    publishSensor("Discharge current limit", nullptr, "settings/dischargeCurrentLimitation", "current", "measurement", "A");
    publishSensor("Discharge voltage limit", nullptr, "settings/dischargeVoltageLimitation", "voltage", "measurement", "V", true, nullptr, 2);

    publishSensor("State of Health (SOH)", "mdi:heart-plus", "stateOfHealth", nullptr, "measurement", "%");
    publishSensor("Temperature", "mdi:thermometer", "temperature", "temperature", "measurement", "°C");

    publishSensor("Charged Energy", nullptr, "chargedEnergy", "energy", "total_increasing", "kWh");
    publishSensor("Discharged Energy", nullptr, "dischargedEnergy", "energy", "total_increasing", "kWh");

    publishSensor("Total Capacity", nullptr, "capacity", nullptr, "measurement", "Ah");
    publishSensor("Available Capacity", nullptr, "availableCapacity", nullptr, "measurement", "Ah");

    publishSensor("Cell Min Voltage", nullptr, "CellMinMilliVolt", "voltage", "measurement", "mV");
    publishSensor("Cell Max Voltage", nullptr, "CellMaxMilliVolt", "voltage", "measurement", "mV");
    publishSensor("Cell Voltage Diff", "mdi:battery-alert", "CellDiffMilliVolt", "voltage", "measurement", "mV");
    publishSensor("Cell Min Temperature", nullptr, "CellMinTemperature", "temperature", "measurement", "°C");
    publishSensor("Cell Max Temperature", nullptr, "CellMaxTemperature", "temperature", "measurement", "°C");

    publishSensor("Cell Min Voltage Label", nullptr, "CellMinVoltageName");
    publishSensor("Cell Max Voltage Label", nullptr, "CellMaxVoltageName");
    publishSensor("Cell Min Temperature Label", nullptr, "CellMinTemperatureName");
    publishSensor("Cell Max Temperature Label", nullptr, "CellMaxTemperatureName");

    publishSensor("Modules Total", "mdi:counter", "modulesTotal");
    // captions define the entity ids: keep them identical to the Pytes CAN provider
    publishSensor("Modules Blocking Charge", "mdi:counter", "modulesBlockingCharge");
    publishSensor("Modules Blocking Discharge", "mdi:counter", "modulesBlockingDischarge");


    publishBinarySensor("Alarm Discharge current", "mdi:alert", "alarm/overCurrentDischarge", "1", "0");
    publishBinarySensor("Alarm High charge current", "mdi:alert", "alarm/overCurrentCharge", "1", "0");
    publishBinarySensor("Alarm Voltage low", "mdi:alert", "alarm/underVoltage", "1", "0");
    publishBinarySensor("Alarm Voltage high", "mdi:alert", "alarm/overVoltage", "1", "0");
    publishBinarySensor("Alarm Temperature low", "mdi:thermometer-low", "alarm/underTemperature", "1", "0");
    publishBinarySensor("Alarm Temperature high", "mdi:thermometer-high", "alarm/overTemperature", "1", "0");
    publishBinarySensor("Alarm Temperature low (charge)", "mdi:thermometer-low", "alarm/underTemperatureCharge", "1", "0");
    publishBinarySensor("Alarm Temperature high (charge)", "mdi:thermometer-high", "alarm/overTemperatureCharge", "1", "0");
    publishBinarySensor("Alarm BMS internal", "mdi:alert", "alarm/bmsInternal", "1", "0");

    publishBinarySensor("Warning Discharge current", "mdi:alert-outline", "warning/highCurrentDischarge", "1", "0");
    publishBinarySensor("Warning High charge current", "mdi:alert-outline", "warning/highCurrentCharge", "1", "0");
    publishBinarySensor("Warning Voltage low", "mdi:alert-outline", "warning/lowVoltage", "1", "0");
    publishBinarySensor("Warning Voltage high", "mdi:alert-outline", "warning/highVoltage", "1", "0");
    publishBinarySensor("Warning Temperature low", "mdi:thermometer-low", "warning/lowTemperature", "1", "0");
    publishBinarySensor("Warning Temperature high", "mdi:thermometer-high", "warning/highTemperature", "1", "0");
    publishBinarySensor("Warning Temperature low (charge)", "mdi:thermometer-low", "warning/lowTemperatureCharge", "1", "0");
    publishBinarySensor("Warning Temperature high (charge)", "mdi:thermometer-high", "warning/highTemperatureCharge", "1", "0");

    publishBinarySensor("Balancing Active", "mdi:scale-balance", "balancingActive", "1", "0");
    publishBinarySensor("Charge immediately", "mdi:alert", "charging/chargeImmediately", "1", "0");
    // Every module is its own HASS device (below the battery device), keyed by
    // its serial: stable when modules are re-wired or added, and globally
    // unique, so it does not depend on how the battery device itself is
    // identified. Per-cell sensors are disabled by default: 3 per cell adds up
    // quickly (16 modules x 16 cells) and bloats the HASS recorder.
    // The provider triggers a republish once all module serials are known.
    for (auto const& id : _spRs485Stats->getModuleIdentities()) {
        // "<manufacturer> <hw version> <serial>": modules usually share the same
        // model, the serial keeps the names (and the entity ids HASS derives
        // from them) distinct
        auto const& manufacturer = _spRs485Stats->getManufacturer();
        String name = manufacturer.has_value() ? *manufacturer : String("Battery");
        name += id.hwVersion.isEmpty() ? String(" Module") : " " + id.hwVersion;
        name += " " + id.serial;

        SubDevice const device {
            "pytes_module_" + id.serial,
            name,
            id.hwVersion,
            id.swVersion,
        };
        auto sensor = [&](String const& caption, char const* icon, String const& topic,
                          char const* deviceClass, char const* stateClass, char const* unit,
                          bool enabled = true, int8_t precision = -1) {
            publishSensor(caption.c_str(), icon, String(id.serial + "/" + topic).c_str(),
                deviceClass, stateClass, unit, enabled, &device, precision);
        };
        sensor("Cell Min Voltage", nullptr, "CellMinMilliVolt", "voltage", "measurement", "mV");
        sensor("Cell Max Voltage", nullptr, "CellMaxMilliVolt", "voltage", "measurement", "mV");
        sensor("Cell Voltage Diff", "mdi:battery-alert", "CellDiffMilliVolt", "voltage", "measurement", "mV");
        sensor("Cell Max Temperature", nullptr, "CellMaxTemperature", "temperature", "measurement", "°C");
        sensor("Power", nullptr, "power", "power", "measurement", "W");
        sensor("Voltage", nullptr, "voltage", "voltage", "measurement", "V", true, 2);
        sensor("Current", nullptr, "current", "current", "measurement", "A");
        sensor("State Of Charge", nullptr, "stateOfCharge", nullptr, "measurement", "%");
        sensor("State Of Health", nullptr, "stateOfHealth", nullptr, "measurement", "%");
        // module values that also exist on battery level: same captions as there
        sensor("Charge voltage (BMS)", nullptr, "settings/chargeVoltage", "voltage", "measurement", "V", true, 2);
        sensor("Charge current limit", nullptr, "settings/chargeCurrentLimitation", "current", "measurement", "A");
        sensor("Discharge current limit", nullptr, "settings/dischargeCurrentLimitation", "current", "measurement", "A");
        sensor("Discharge voltage limit", nullptr, "settings/dischargeVoltageLimitation", "voltage", "measurement", "V", true, 2);
        sensor("Temperature", "mdi:thermometer", "temperature", "temperature", "measurement", "°C");
        sensor("Total Capacity", nullptr, "capacity", nullptr, "measurement", "Ah");
        sensor("Available Capacity", nullptr, "availableCapacity", nullptr, "measurement", "Ah");
        sensor("Cell Min Temperature", nullptr, "CellMinTemperature", "temperature", "measurement", "°C");
        sensor("Charge Cycles", "mdi:counter", "chargeCycles", nullptr, nullptr, nullptr);
        publishBinarySensor("Balancing Active", "mdi:scale-balance", String(id.serial + "/balancingActive").c_str(), "1", "0", true, &device);
        publishBinarySensor("Charge immediately", "mdi:alert", String(id.serial + "/charging/chargeImmediately").c_str(), "1", "0", true, &device);
        publishBinarySensor("Full charge requested", "mdi:battery-sync", String(id.serial + "/charging/fullChargeRequest").c_str(), "1", "0", true, &device);

        // cell count comes from the module info (0x80), read right before this republish
        for (uint8_t c = 1; c <= id.nCells; ++c) {
            // explicitly String: "auto" would deduce Arduino's StringSumHelper,
            // whose operator+ appends in place and makes the captions pile up
            String const cell = "Cell " + String(c) + " ";
            String const topic = "cell/" + String(c) + "/";
            sensor(cell + "Voltage", nullptr, topic + "voltage", "voltage", "measurement", "V", false, 3);
            sensor(cell + "Temperature", nullptr, topic + "temperature", "temperature", "measurement", "°C", false);
            sensor(cell + "State Of Charge", nullptr, topic + "stateOfCharge", nullptr, "measurement", "%", false);
        }
    }

    publishBinarySensor("Full charge requested", "mdi:battery-sync", "charging/fullChargeRequest", "1", "0");
}

} // namespace Batteries::Pytes::Rs485
