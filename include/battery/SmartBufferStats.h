// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <battery/Stats.h>
#include <solarcharger/Controller.h>
#include <solarcharger/integrated/Provider.h>
#include <solarcharger/integrated/Stats.h>

namespace Batteries {

// mandatory interface for all kinds of batteries
class SmartBufferStats : public Stats {

public:
    using MPPT = SolarChargers::Integrated::MPPT;

    virtual std::optional<String> const& getDeviceName() const = 0;
    virtual size_t getNumberMppts() const = 0;

    inline std::optional<float> getSolarPowerOverall() const {
        return _solar_power_sum;
    }

    inline std::optional<float> getSolarPower(const MPPT mppt) const {
        try {
            return _solar_power.at(mppt);
        }
        catch(const std::out_of_range& ex) {;}

        return std::nullopt;
    }

    inline std::optional<float> getSolarVoltage(const MPPT mppt) const {
        try {
            return _solar_voltage.at(mppt);
        }
        catch(const std::out_of_range& ex) {;}

        return std::nullopt;
    }

protected:
    inline void setSolarPower(const MPPT mppt, const std::optional<float> &power, const uint32_t timestamp = millis()) {
        if (!power.has_value()) { return; }
        _solar_power[mppt] = power;

        // update sum value
        std::optional<float> sum = std::nullopt;
        for (const auto& [index, power] : _solar_power) {
            if (!power.has_value()) { continue; }
            sum = sum.value_or(0) + *power;
        }
        _solar_power_sum = sum;

        // publish value to SolcarCharger integration
        auto object = getObject(mppt);
        if (object == nullptr ) { return; }
        object->setPower(*power, timestamp);
    }

    inline void setSolarVoltage(const MPPT mppt, const std::optional<float> &voltage, const uint32_t timestamp = millis()) {
        if (!voltage.has_value()) { return; }
        _solar_voltage[mppt] = voltage;

        // publish value to SolcarCharger integration
        auto object = getObject(mppt);
        if (object == nullptr) { return; }
        object->setVoltage(*voltage, timestamp);
    }

private:
    std::shared_ptr<SolarChargers::Integrated::MpptData> getObject(const MPPT mppt)
    {
        try {
            return getObjects().at(mppt);
        }
        catch(const std::out_of_range& ex) {;}

        return nullptr;
    }


    std::map<MPPT, std::shared_ptr<SolarChargers::Integrated::MpptData>> getObjects()
    {
        auto mppt = SolarCharger.getIntegratedStats();
        if (mppt == nullptr) { return {}; }

        if (mppt->hasDevice(_solarcharger_id)) { return _objects; }

        // if device was not found, immediatly clear our cached pointers
        _objects.clear();

        // amd try to re-add our device
        auto result = mppt->addDevice(getManufacturer(), getDeviceName(), getSerial(), getNumberMppts());
        if (!result.has_value()) { return {}; }

        _solarcharger_id = (*result).first;
        _objects = (*result).second->getMppts();

        return _objects;
    }

    std::optional<uint32_t> _solarcharger_id = std::nullopt;
    std::optional<float> _solar_power_sum = std::nullopt;
    std::map<MPPT, std::shared_ptr<SolarChargers::Integrated::MpptData>> _objects = std::map<MPPT, std::shared_ptr<SolarChargers::Integrated::MpptData>>();
    std::map<MPPT, std::optional<float>> _solar_power = std::map<MPPT, std::optional<float>>();
    std::map<MPPT, std::optional<float>> _solar_voltage = std::map<MPPT, std::optional<float>>();
};

} // namespace Batteries
