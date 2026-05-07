// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <battery/HassIntegration.h>
#include <battery/pytes/rs485/Stats.h>

namespace Batteries::Pytes::Rs485 {

class HassIntegration : public ::Batteries::HassIntegration {
public:
    explicit HassIntegration(std::shared_ptr<Stats> spStats);
    void publishSensors() const final;
};

} // namespace Batteries::Pytes::Rs485
