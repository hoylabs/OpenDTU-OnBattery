// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <powermeter/Provider.h>
#include <TaskSchedulerDeclarations.h>
#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>

namespace PowerMeters {

class Controller {
public:
    struct ChannelStats {
        std::optional<float> Raw;
        std::optional<float> Average;
        std::optional<float> Last;
        std::optional<float> Minimum;
        std::optional<float> Maximum;
    };

    struct PowerStats {
        ChannelStats Total;
        ChannelStats L1;
        ChannelStats L2;
        ChannelStats L3;
    };

    void init(Scheduler& scheduler);

    void updateSettings();

    float getPowerTotal() const;
    PowerStats getPowerStats() const;
    uint32_t getLastUpdate() const;
    bool isDataValid() const;

private:
    enum class Channel : uint8_t { Total = 0, L1 = 1, L2 = 2, L3 = 3 };

    struct Sample {
        uint32_t Timestamp;
        float Value;
    };

    struct ChannelWindow {
        std::deque<Sample> Samples;
        float Sum = 0.0f;
    };

    struct ChannelState {
        ChannelWindow Window;
        ChannelStats Stats;
    };

    void resetPowerStats();
    void updatePowerStatsFromProvider();
    void addSample(Channel channel, float value, uint32_t timestamp);
    void pruneWindow(ChannelWindow& window, uint32_t now) const;
    void updateStats(ChannelState& state);
    void refreshAgingForTimeBasedWindows();
    ChannelState& channelState(Channel channel);
    ChannelState const& channelState(Channel channel) const;
    void sanitizeAveragingConfig(PowerMeterAveragingConfig& cfg) const;

    void loop();

    Task _loopTask;
    mutable std::mutex _mutex;
    std::unique_ptr<Provider> _upProvider = nullptr;
    uint32_t _lastProviderUpdate = 0;
    PowerMeterAveragingConfig _averagingCfg = {};
    std::array<ChannelState, 4> _powerStats = {};
};

} // namespace PowerMeters

extern PowerMeters::Controller PowerMeter;
