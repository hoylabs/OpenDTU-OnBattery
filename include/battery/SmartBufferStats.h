// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <battery/Stats.h>
#include <solarcharger/Controller.h>
#include <solarcharger/smartbufferbatteries/Provider.h>
#include <solarcharger/smartbufferbatteries/Stats.h>

namespace Batteries {

// mandatory interface for all kinds of batteries
class SmartBufferStats : public Stats {
public:
    virtual std::optional<String> const& getDeviceName() const = 0;
    virtual size_t getNumberMppts() const = 0;

protected:
    void setMpptPower(const size_t mppt, const float power, const uint32_t timestamp = millis())
    {
        auto charger = getSolarCharger();
        if (charger != nullptr) {
            charger->setMpptPower(_solarcharger_id, mppt, power, timestamp);
        }
    }

    void setMpptVoltage(const size_t mppt, const float voltage, const uint32_t timestamp = millis())
    {
        auto charger = getSolarCharger();
        if (charger != nullptr) {
            charger->setMpptVoltage(_solarcharger_id, mppt, voltage, timestamp);
        }
    }

    std::shared_ptr<SolarChargers::SmartBufferBatteries::Stats> getSolarCharger()
    {
        auto mppt = SolarCharger.getSmartBufferBatteryStats();
        if (mppt != nullptr && !mppt->hasDevice(_solarcharger_id)) {
            _solarcharger_id = mppt->addDevice(getManufacturer(), getDeviceName(), getSerial(), getNumberMppts());
        }

        return mppt;
    }

    std::optional<uint32_t> _solarcharger_id = std::nullopt;

};

} // namespace Batteries
