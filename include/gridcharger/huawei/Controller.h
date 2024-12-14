// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <memory>
#include <TaskSchedulerDeclarations.h>
#include <gridcharger/huawei/MCP2515.h>

namespace GridCharger::Huawei {

#define HUAWEI_MINIMAL_OFFLINE_VOLTAGE 48
#define HUAWEI_MINIMAL_ONLINE_VOLTAGE 42

#define MAX_CURRENT_MULTIPLIER 20

// Index values for rec_values array
#define HUAWEI_INPUT_POWER_IDX 0
#define HUAWEI_INPUT_FREQ_IDX 1
#define HUAWEI_INPUT_CURRENT_IDX 2
#define HUAWEI_OUTPUT_POWER_IDX 3
#define HUAWEI_EFFICIENCY_IDX 4
#define HUAWEI_OUTPUT_VOLTAGE_IDX 5
#define HUAWEI_OUTPUT_CURRENT_MAX_IDX 6
#define HUAWEI_INPUT_VOLTAGE_IDX 7
#define HUAWEI_OUTPUT_TEMPERATURE_IDX 8
#define HUAWEI_INPUT_TEMPERATURE_IDX 9
#define HUAWEI_OUTPUT_CURRENT_IDX 10
#define HUAWEI_OUTPUT_CURRENT1_IDX 11

// Defines and index values for tx_values array
#define HUAWEI_OFFLINE_VOLTAGE 0x01
#define HUAWEI_ONLINE_VOLTAGE 0x00
#define HUAWEI_OFFLINE_CURRENT 0x04
#define HUAWEI_ONLINE_CURRENT 0x03

// Modes of operation
#define HUAWEI_MODE_OFF 0
#define HUAWEI_MODE_ON 1
#define HUAWEI_MODE_AUTO_EXT 2
#define HUAWEI_MODE_AUTO_INT 3

// Error codes
#define HUAWEI_ERROR_CODE_RX 0x01
#define HUAWEI_ERROR_CODE_TX 0x02

// Wait time/current before shuting down the PSU / charger
// This is set to allow the fan to run for some time
#define HUAWEI_AUTO_MODE_SHUTDOWN_DELAY 60000
#define HUAWEI_AUTO_MODE_SHUTDOWN_CURRENT 0.75

// Updateinterval used to request new values from the PSU
#define HUAWEI_DATA_REQUEST_INTERVAL_MS 2500

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
