// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <Arduino.h>
#include <AsyncJson.h>

namespace GridChargers {

class Stats {
public:
    virtual uint32_t getLastUpdate() const;

    virtual std::optional<float> getInputPower() const;

    // convert stats to JSON for web application live view
    virtual void getLiveViewData(JsonVariant& root) const;

    void mqttLoop();

    // the interval at which all data will be re-published, even
    // if they did not change. used to calculate Home Assistent expiration.
    uint32_t getMqttFullPublishIntervalMs() const;

protected:
    virtual void mqttPublish() const;

    template<typename T>
    void addValueInSection(JsonVariant& root,
        std::string const& section, std::string const& name,
        T value, std::string const& unit,
        int precision = 2) const
    {
        auto jsonValue = root["values"][section][name];
        jsonValue["v"] = value;
        jsonValue["u"] = unit;
        jsonValue["d"] = precision;
    }

    template<typename T>
    void addStringInSection(JsonVariant& root,
        std::string const& section, std::string const& name,
        T value, bool translate = true) const
    {
        auto jsonValue = root["values"][section][name];
        jsonValue["value"] = value;
        jsonValue["translate"] = translate;
    }

private:
    uint32_t _lastMqttPublish = 0;
};

} // namespace GridChargers
