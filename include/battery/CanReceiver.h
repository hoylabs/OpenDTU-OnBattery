// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>
#include <driver/twai.h>
#include <battery/Provider.h>

namespace Batteries {

class CanReceiver : public Provider {
public:
    bool init(char const* providerName);
    void deinit() final;
    void loop() final;

    virtual void onMessage(twai_message_t rx_message) = 0;

protected:
    // called on every loop, e.g. to send requests to the battery
    virtual void onLoop() { }

    // single shot, does not block. returns false if the frame was not queued.
    bool transmit(uint32_t identifier, uint8_t const* data, uint8_t length, bool extended = false);

    uint8_t readUnsignedInt8(uint8_t *data);
    uint16_t readUnsignedInt16(uint8_t *data);
    int16_t readSignedInt16(uint8_t *data);
    uint32_t readUnsignedInt32(uint8_t *data);
    int32_t readSignedInt24(uint8_t *data);
    float scaleValue(int32_t value, float factor);
    bool getBit(uint32_t value, uint8_t bit);

private:
    char const* _providerName = "Battery CAN";
};

} // namespace Batteries
