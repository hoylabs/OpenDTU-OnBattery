// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <Arduino.h>

namespace SolarChargers {

class Stats {
public:
    void mqttLoop();

    // the interval at which all data will be re-published, even
    // if they did not change. used to calculate Home Assistent expiration.
    virtual uint32_t getMqttFullPublishIntervalMs() const;

protected:
    virtual void mqttPublish() const;

private:
    uint32_t _lastMqttPublish = 0;
};

} // namespace SolarChargers
