// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <memory>
#include <ArduinoJson.h>
#include <TaskSchedulerDeclarations.h>
#include <gridcharger/huawei/HardwareInterface.h>
#include <gridcharger/huawei/DataPoints.h>

namespace GridCharger::Huawei {

#define HUAWEI_MINIMAL_OFFLINE_VOLTAGE 48
#define HUAWEI_MINIMAL_ONLINE_VOLTAGE 42

// Modes of operation
#define HUAWEI_MODE_OFF 0
#define HUAWEI_MODE_ON 1
#define HUAWEI_MODE_AUTO_EXT 2
#define HUAWEI_MODE_AUTO_INT 3

class Controller {
public:
    void init(Scheduler& scheduler);
    void updateSettings();
    void setParameter(float val, HardwareInterface::Setting setting);
    void setMode(uint8_t mode);

    DataPointContainer const& getDataPoints() const { return _dataPoints; }
    void getJsonData(JsonVariant& root) const;

    bool getAutoPowerStatus() const { return _autoPowerEnabled; };
    uint8_t getMode() const { return _mode; };

private:
    void loop();
    void _setParameter(float val, HardwareInterface::Setting setting);

    // these control the pin named "power", which in turn is supposed to control
    // a relay (or similar) to enable or disable the PSU using it's slot detect
    // pins.
    void enableOutput();
    void disableOutput();
    int8_t _huaweiPower;

    Task _loopTask;
    std::unique_ptr<HardwareInterface> _upHardwareInterface;

    std::mutex _mutex;
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
