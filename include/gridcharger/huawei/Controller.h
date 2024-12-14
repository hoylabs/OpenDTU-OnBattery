// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <memory>
#include <TaskSchedulerDeclarations.h>
#include <gridcharger/huawei/MCP2515.h>

namespace GridCharger::Huawei {

#define HUAWEI_MINIMAL_OFFLINE_VOLTAGE 48
#define HUAWEI_MINIMAL_ONLINE_VOLTAGE 42

// Modes of operation
#define HUAWEI_MODE_OFF 0
#define HUAWEI_MODE_ON 1
#define HUAWEI_MODE_AUTO_EXT 2
#define HUAWEI_MODE_AUTO_INT 3

typedef struct RectifierParameters {
    float input_voltage;
    float input_frequency;
    float input_current;
    float input_power;
    float input_temp;
    float efficiency;
    float output_voltage;
    float output_current;
    float max_output_current;
    float output_power;
    float output_temp;
    float amp_hour;
} RectifierParameters_t;

class Controller {
public:
    void init(Scheduler& scheduler, uint8_t huawei_miso, uint8_t huawei_mosi, uint8_t huawei_clk, uint8_t huawei_irq, uint8_t huawei_cs, uint8_t huawei_power);
    void updateSettings(uint8_t huawei_miso, uint8_t huawei_mosi, uint8_t huawei_clk, uint8_t huawei_irq, uint8_t huawei_cs, uint8_t huawei_power);
    void setValue(float in, uint8_t parameterType);
    void setMode(uint8_t mode);

    RectifierParameters_t * get();
    uint32_t getLastUpdate() const { return _lastUpdateReceivedMillis; };
    bool getAutoPowerStatus() const { return _autoPowerEnabled; };
    uint8_t getMode() const { return _mode; };

private:
    void loop();
    void processReceivedParameters();
    void _setValue(float in, uint8_t parameterType);

    Task _loopTask;
    std::unique_ptr<MCP2515> _upHardwareInterface;

    bool _initialized = false;
    uint8_t _huaweiPower;           // Power pin
    uint8_t _mode = HUAWEI_MODE_AUTO_EXT;

    RectifierParameters_t _rp;

    uint32_t _lastUpdateReceivedMillis;           // Timestamp for last data seen from the PSU
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
