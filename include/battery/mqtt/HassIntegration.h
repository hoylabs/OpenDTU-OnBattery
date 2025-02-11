// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <battery/HassIntegration.h>

namespace Batteries::Mqtt {

class HassIntegration : public ::Batteries::HassIntegration {
public:
    void publishSensors() const final
    {
        ::Batteries::HassIntegration::publishSensors();
    }
};

} // namespace Batteries::Mqtt
