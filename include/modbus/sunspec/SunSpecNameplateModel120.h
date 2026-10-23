// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <memory>

class InverterAbstract;

// Model 120: Nameplate ratings.
//
// Spec: https://github.com/sunspec/models/blob/master/json/model_120.json
class SunSpecNameplateModel120 {
public:
    static constexpr uint16_t kLength = 26;

    static uint16_t id() { return 120; }
    static uint16_t length() { return kLength; }
    static void fill(uint16_t* payload, std::shared_ptr<InverterAbstract> const& inv, uint8_t unitId);
};
