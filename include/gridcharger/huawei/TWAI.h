// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <gridcharger/huawei/HardwareInterface.h>

namespace GridCharger::Huawei {

class TWAI : public HardwareInterface {
public:
    bool init() final;

    bool getMessage(HardwareInterface::can_message_t& msg) final;

    bool sendMessage(uint32_t valueId, std::array<uint8_t, 8> const& data) final;
};

} // namespace GridCharger::Huawei
