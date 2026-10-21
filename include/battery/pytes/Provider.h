// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <memory>
#include <driver/twai.h>
#include <battery/CanReceiver.h>
#include <battery/pytes/Stats.h>
#include <battery/pytes/HassIntegration.h>

namespace Batteries::Pytes {

class Provider : public ::Batteries::CanReceiver {
public:
    Provider();
    bool init() final;
    void onMessage(twai_message_t rx_message) final;

    std::shared_ptr<::Batteries::Stats> getStats() const final { return _stats; }
    std::shared_ptr<::Batteries::HassIntegration> getHassIntegration() final { return _hassIntegration; }

private:
    void onLoop() final;
    void onModuleCellFrame(uint32_t identifier, uint8_t const* data, uint8_t length);
    void onModuleVersionFrame(uint32_t identifier, uint8_t const* data, uint8_t length);

    std::shared_ptr<Stats> _stats;
    std::shared_ptr<HassIntegration> _hassIntegration;

    // per module data requests, see onLoop()
    uint8_t _nextModule = 1;
    uint8_t _pendingModule = 0; // module whose cell data was requested, 0: none
    uint32_t _lastRequest = 0;
    uint32_t _lastPytesFrame = 0; // when a Pytes-protocol frame was last seen
    uint8_t _unansweredRequests = 0; // consecutive, see onLoop()
    uint16_t _cellFrames = 0; // cell-data subframes received, bit n = subframe n
    uint8_t _cellFramesModule = 0; // module the _cellFrames mask belongs to
    std::array<uint8_t, Stats::MaxModules> _versionRequests = {}; // attempts
    uint8_t _versionModule = 0; // module whose version string is received
    uint8_t _versionLength = 0;
    uint8_t _versionChunks = 0; // version chunks received, bit i = 0x19A62 + i
    std::array<char, 32> _versionBuffer = {};
};

} // namespace Batteries::Pytes
