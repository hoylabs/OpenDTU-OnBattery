// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <battery/HassIntegration.h>
#include <battery/pylontech/Stats.h>

namespace Batteries::Pylontech {

class HassIntegration : public ::Batteries::HassIntegration {
public:
    explicit HassIntegration(std::shared_ptr<Stats> spStats);

    void publishSensors() const final;
};

} // namespace Batteries::Pylontech
