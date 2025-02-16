// SPDX-License-Identifier: GPL-2.0-or-later

#include <battery/victronsmartshunt/HassIntegration.h>

namespace Batteries::VictronSmartShunt {

HassIntegration::HassIntegration(std::shared_ptr<Stats> spStats)
    : ::Batteries::HassIntegration(spStats) { }

void HassIntegration::publishSensors() const
{
    ::Batteries::HassIntegration::publishSensors();

    publishSensor("Instantaneous Power", NULL, "instantaneousPower", "power", "measurement", "W");
    publishSensor("Charged Energy", NULL, "chargedEnergy", "energy", "total_increasing", "kWh");
    publishSensor("Discharged Energy", NULL, "dischargedEnergy", "energy", "total_increasing", "kWh");
    publishSensor("Charge Cycles", "mdi:counter", "chargeCycles");
    publishSensor("Consumed Amp Hours", NULL, "consumedAmpHours", NULL, "measurement", "Ah");
    publishSensor("Last Full Charge", "mdi:timelapse", "lastFullCharge", NULL, NULL, "min");
    publishSensor("Midpoint Voltage", NULL, "midpointVoltage", "voltage", "measurement", "V");
    publishSensor("Midpoint Deviation", NULL, "midpointDeviation", "battery", "measurement", "%");
}

} // namespace Batteries::VictronSmartShunt
