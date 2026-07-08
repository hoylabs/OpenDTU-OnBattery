// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <memory>

class InverterAbstract;

// Model 1: Common. Manufacturer/model/serial/device-address identification.
// Mandatory as the first model for every SunSpec compliant device.
//
// Spec: https://github.com/sunspec/models/blob/master/json/model_1.json
class SunSpecCommonModel1 {
public:
    static constexpr uint16_t kLength = 66;

    static uint16_t id() { return 1; }
    static uint16_t length() { return kLength; }
    static void fill(uint16_t* payload, std::shared_ptr<InverterAbstract> const& inv, uint8_t unitId);
};
