// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <atomic>
#include <gridcharger/Provider.h>
#include <gridcharger/HTTP/Charger.h>
#include <gridcharger/HTTP/Stats.h>
#include <memory>
#include <mutex>
#include <vector>
#include <condition_variable>

namespace GridChargers::HTTP {

class Provider : public ::GridChargers::Provider {
public:
    bool init() final;
    void deinit() final;
    void loop() final;
    std::shared_ptr<::GridChargers::Stats> getStats() const final { return _stats; }

    // always false: the HTTP chargers never block the battery-powered inverters
    bool getAutoPowerStatus() const final;

private:
    void powerControlLoop();

    // switches all chargers, ignoring the minimum on/off time
    void switchAll(bool on);
    bool isAnyChargerOn() const;

    // evaluates the battery voltage stop/resume thresholds (with hysteresis)
    bool isBatteryVoltageStopActive();

    // selects and applies the combination of chargers which uses the most
    // surplus power without exceeding the target consumption. returns true
    // if any charger was switched.
    bool applyBestCombination(float powerTotal, float targetConsumption);

    static void dataPollingLoopHelper(void* context);
    void dataPollingLoop();
    void pollData();

    std::vector<std::shared_ptr<Charger>> _chargers;

    TaskHandle_t _dataPollingTaskHandle = nullptr;
    std::atomic<bool> _dataPollingTaskDone = false;
    bool _stopPollingData = false;
    mutable std::mutex _dataPollingMutex;
    std::condition_variable _dataPollingCv;
    uint32_t _lastDataPoll = 0;

    static constexpr int DATA_POLLING_INTERVAL_MS = 5000; // 5 seconds

    // only switch to a combination with more power if it gains at least this much
    static constexpr float SWITCH_HYSTERESIS_W = 50.0f;

    // time to wait for output and power meter stabilization after switching
    static constexpr uint32_t STABILIZATION_TIME_MS = 30 * 1000;

    std::shared_ptr<Stats> _stats = std::make_shared<Stats>();

    uint32_t _lastPowerMeterUpdateReceivedMillis = 0;
    uint32_t _autoModeBlockedSinceMillis = 0;
    uint32_t _autoModeBlockedForMillis = 0;
    uint32_t _lastSoCLogMillis = 0;

    bool _batteryEmergencyCharging = false;
    bool _batteryVoltageStopActive = false;
};

} // namespace GridChargers::HTTP
