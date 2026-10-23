// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <memory>

class InverterAbstract;

// Model 101 (single-phase) / 103 (three-phase): AC measurements.
//
// Spec: https://github.com/sunspec/models/blob/master/json/model_101.json
//       https://github.com/sunspec/models/blob/master/json/model_103.json
class SunSpecInverterModel101_103 {
public:
    static constexpr uint16_t kLength = 50;

    static uint16_t id(std::shared_ptr<InverterAbstract> const& inv);
    static uint16_t length() { return kLength; }
    static void fill(uint16_t* payload, std::shared_ptr<InverterAbstract> const& inv, uint8_t unitId);
};
