// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "HoymilesRadio.h"

// Stub "radio" used for WiFi-connected HMS-xxxxW-2T inverters.
// These inverters communicate via TCP on port 10081 instead of NRF24/CMT radio.
// This class satisfies the HoymilesRadio interface expected by InverterAbstract
// while keeping the command queue empty at all times.
class HoymilesRadio_WiFi : public HoymilesRadio {
public:
    HoymilesRadio_WiFi() { _isInitialized = true; }

protected:
    void sendEsbPacket(CommandAbstract& cmd) override { (void)cmd; }
};
