// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <ArduinoJson.h>
#include <TaskSchedulerDeclarations.h>
#include <gridcharger/huawei/HardwareInterface.h>
#include <gridcharger/huawei/DataPoints.h>

namespace GridCharger::Huawei {

// Modes of operation
#define HUAWEI_MODE_OFF 0
#define HUAWEI_MODE_ON 1
#define HUAWEI_MODE_AUTO_EXT 2
#define HUAWEI_MODE_AUTO_INT 3

class Controller {
public:
    void init(Scheduler& scheduler);
    void updateSettings();
    void setFan(bool online, bool fullSpeed);
    void setProduction(bool enable);
    void setParameter(float val, HardwareInterface::Setting setting);
    void setMode(uint8_t mode);

    DataPointContainer const& getDataPoints() const { return _dataPoints; }
    void getJsonData(JsonVariant& root) const;

    bool getAutoPowerStatus() const { return _autoPowerEnabled; };
    uint8_t getMode() const { return _mode; };

    // determined through trial and error (voltage limits, R4850G2)
    // and some educated guessing (current limits, no R4875 at hand)
    static constexpr float MIN_ONLINE_VOLTAGE = 41.0f;
    static constexpr float MAX_ONLINE_VOLTAGE = 58.6f;
    static constexpr float MIN_ONLINE_CURRENT = 0.0f;
    static constexpr float MAX_ONLINE_CURRENT = 84.0f;
    static constexpr float MIN_OFFLINE_VOLTAGE = 48.0f;
    static constexpr float MAX_OFFLINE_VOLTAGE = 58.4f;
    static constexpr float MIN_OFFLINE_CURRENT = 0.0f;
    static constexpr float MAX_OFFLINE_CURRENT = 84.0f;
    static constexpr float MIN_INPUT_CURRENT_LIMIT = 0.0f;
    static constexpr float MAX_INPUT_CURRENT_LIMIT = 40.0f;

private:
    void loop();
    void _setParameter(float val, HardwareInterface::Setting setting);
    void _setProduction(bool enable);

    // these control the pin named "power", which in turn is supposed to control
    // a relay (or similar) to enable or disable the PSU using it's slot detect
    // pins.
    void enableOutput();
    void disableOutput();
    gpio_num_t _huaweiPower;

    Task _loopTask;
    std::unique_ptr<HardwareInterface> _upHardwareInterface;

    std::mutex _mutex;
    std::optional<bool> _oOutputEnabled;
    uint8_t _mode = HUAWEI_MODE_AUTO_EXT;

    DataPointContainer _dataPoints;

    uint32_t _outputCurrentOnSinceMillis;         // Timestamp since when the PSU was idle at zero amps
    uint32_t _nextAutoModePeriodicIntMillis;      // When to set the next output voltage in automatic mode
    uint32_t _lastPowerMeterUpdateReceivedMillis; // Timestamp of last seen power meter value
    uint32_t _autoModeBlockedTillMillis = 0;      // Timestamp to block running auto mode for some time

    uint8_t _autoPowerEnabledCounter = 0;
    bool _autoPowerEnabled = false;
    bool _batteryEmergencyCharging = false;
};

} // namespace GridCharger::Huawei

extern GridCharger::Huawei::Controller HuaweiCan;
