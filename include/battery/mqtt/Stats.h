// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <battery/Stats.h>

namespace Batteries::Mqtt {

class Stats : public ::Batteries::Stats {
friend class Provider;

public:
    // since the source of information was MQTT in the first place,
    // we do NOT publish the same data under a different topic.
    void mqttPublish() const final { }

    bool supportsAlarmsAndWarnings() const final { return false; }
};

} // namespace Batteries::Mqtt
