// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <battery/HassIntegration.h>

namespace Batteries::JkBms {

class HassIntegration : public ::Batteries::HassIntegration {
public:
    void publishSensors() const final;
};

} // namespace Batteries::JkBms
