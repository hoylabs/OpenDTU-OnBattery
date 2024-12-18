// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Malte Schmidt and others
 */
#include "Battery.h"
#include <gridcharger/huawei/Controller.h>
#include <gridcharger/huawei/MCP2515.h>
#include <gridcharger/huawei/TWAI.h>
#include "MessageOutput.h"
#include "PowerMeter.h"
#include "PowerLimiter.h"
#include "Configuration.h"

#include <functional>
#include <algorithm>

GridCharger::Huawei::Controller HuaweiCan;

namespace GridCharger::Huawei {

// Wait time/current before shuting down the PSU / charger
// This is set to allow the fan to run for some time
#define HUAWEI_AUTO_MODE_SHUTDOWN_DELAY 60000
#define HUAWEI_AUTO_MODE_SHUTDOWN_CURRENT 0.75

void Controller::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&Controller::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.enable();

    updateSettings();
}

void Controller::enableOutput()
{
    if (_huaweiPower < 0) { return; }
    digitalWrite(_huaweiPower, 0);
}

void Controller::disableOutput()
{
    if (_huaweiPower < 0) { return; }
    digitalWrite(_huaweiPower, 1);
}

void Controller::updateSettings()
{
    std::lock_guard<std::mutex> lock(_mutex);

    _upHardwareInterface.reset(nullptr);

    auto const& config = Configuration.get();

    if (!config.Huawei.Enabled) { return; }

    switch (config.Huawei.HardwareInterface) {
        case GridChargerHardwareInterface::MCP2515:
            _upHardwareInterface = std::make_unique<MCP2515>();
            break;
        case GridChargerHardwareInterface::VP230:
            _upHardwareInterface = std::make_unique<TWAI>();
            break;
        default:
            MessageOutput.printf("[HuaweiCanClass::init] Unknown hardware "
                    "interface setting %d\r\n", config.Huawei.HardwareInterface);
            return;
            break;
    }

    if (!_upHardwareInterface->init()) {
        MessageOutput.println("[HuaweiCanClass::init] Error initializing hardware interface");
        _upHardwareInterface.reset(nullptr);
        return;
    };

    auto const& pin = PinMapping.get();
    if (pin.huawei_power >= 0) {
        _huaweiPower = pin.huawei_power;
        pinMode(_huaweiPower, OUTPUT);
        disableOutput();
    }

    if (config.Huawei.Auto_Power_Enabled) {
        _mode = HUAWEI_MODE_AUTO_INT;
    }

    MessageOutput.println("[HuaweiCanClass::init] Hardware Interface initialized successfully");
}

RectifierParameters_t * Controller::get()
{
    return &_rp;
}

void Controller::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (!_upHardwareInterface) { return; }

    auto const& config = Configuration.get();

    bool verboseLogging = config.Huawei.VerboseLogging;

    uint32_t lastUpdate = _lastUpdateReceivedMillis;
    _lastUpdateReceivedMillis = std::numeric_limits<uint32_t>::max();
    using Prop = HardwareInterface::Property;
    auto assign = [this](float* pTarget, Prop p) {
        auto [val, timestamp] = _upHardwareInterface->getParameter(p);
        _lastUpdateReceivedMillis = std::min(_lastUpdateReceivedMillis, timestamp);
        *pTarget = val;
    };
    assign(&_rp.input_power, Prop::InputPower);
    assign(&_rp.input_frequency, Prop::InputFrequency);
    assign(&_rp.input_current, Prop::InputCurrent);
    assign(&_rp.output_power, Prop::OutputPower);
    assign(&_rp.efficiency, Prop::Efficiency);
    assign(&_rp.output_voltage, Prop::OutputVoltage);
    assign(&_rp.max_output_current, Prop::OutputCurrentMax);
    assign(&_rp.input_voltage, Prop::InputVoltage);
    assign(&_rp.output_temp, Prop::OutputTemperature);
    assign(&_rp.input_temp, Prop::InputTemperature);
    assign(&_rp.output_current, Prop::OutputCurrent);

    // Print updated data
    if (lastUpdate != _lastUpdateReceivedMillis && verboseLogging) {
        MessageOutput.printf("[HuaweiCanClass::loop] In:  %.02fV, %.02fA, %.02fW\n", _rp.input_voltage, _rp.input_current, _rp.input_power);
        MessageOutput.printf("[HuaweiCanClass::loop] Out: %.02fV, %.02fA of %.02fA, %.02fW\n", _rp.output_voltage, _rp.output_current, _rp.max_output_current, _rp.output_power);
        MessageOutput.printf("[HuaweiCanClass::loop] Eff : %.01f%%, Temp in: %.01fC, Temp out: %.01fC\n", _rp.efficiency * 100, _rp.input_temp, _rp.output_temp);
    }

    // Internal PSU power pin (slot detect) control
    if (_rp.output_current > HUAWEI_AUTO_MODE_SHUTDOWN_CURRENT) {
        _outputCurrentOnSinceMillis = millis();
    }

    if (_outputCurrentOnSinceMillis + HUAWEI_AUTO_MODE_SHUTDOWN_DELAY < millis() &&
            (_mode == HUAWEI_MODE_AUTO_EXT || _mode == HUAWEI_MODE_AUTO_INT)) {
        disableOutput();
    }

    using Setting = HardwareInterface::Setting;

    if (_mode == HUAWEI_MODE_AUTO_INT || _batteryEmergencyCharging) {

        // Set voltage limit in periodic intervals if we're in auto mode or if emergency battery charge is requested.
        if ( _nextAutoModePeriodicIntMillis < millis()) {
            MessageOutput.printf("[HuaweiCanClass::loop] Periodically setting voltage limit: %f \r\n", config.Huawei.Auto_Power_Voltage_Limit);
            _setParameter(config.Huawei.Auto_Power_Voltage_Limit, Setting::OnlineVoltage);
            _nextAutoModePeriodicIntMillis = millis() + 60000;
        }
    }

    // ***********************
    // Emergency charge
    // ***********************
    auto stats = Battery.getStats();
    if (config.Huawei.Emergency_Charge_Enabled && stats->getImmediateChargingRequest()) {
        _batteryEmergencyCharging = true;

        // Set output current
        float efficiency =  (_rp.efficiency > 0.5 ? _rp.efficiency : 1.0);
        float outputCurrent = efficiency * (config.Huawei.Auto_Power_Upper_Power_Limit / _rp.output_voltage);
        MessageOutput.printf("[HuaweiCanClass::loop] Emergency Charge Output current %f \r\n", outputCurrent);
        _setParameter(outputCurrent, Setting::OnlineCurrent);
        return;
    }

    if (_batteryEmergencyCharging && !stats->getImmediateChargingRequest()) {
        // Battery request has changed. Set current to 0, wait for PSU to respond and then clear state
        _setParameter(0, Setting::OnlineCurrent);
        if (_rp.output_current < 1) {
            _batteryEmergencyCharging = false;
        }
        return;
    }

    // ***********************
    // Automatic power control
    // ***********************
    if (_mode == HUAWEI_MODE_AUTO_INT ) {

        // Check if we should run automatic power calculation at all.
        // We may have set a value recently and still wait for output stabilization
        if (_autoModeBlockedTillMillis > millis()) {
            return;
        }

        // Re-enable automatic power control if the output voltage has dropped below threshold
        if (_rp.output_voltage < config.Huawei.Auto_Power_Enable_Voltage_Limit ) {
            _autoPowerEnabledCounter = 10;
        }

        if (PowerLimiter.isGovernedInverterProducing()) {
            _setParameter(0.0, Setting::OnlineCurrent);
            // Don't run auto mode for a second now. Otherwise we may send too much over the CAN bus
            _autoModeBlockedTillMillis = millis() + 1000;
            MessageOutput.printf("[HuaweiCanClass::loop] Inverter is active, disable\r\n");
            return;
        }

        if (PowerMeter.getLastUpdate() > _lastPowerMeterUpdateReceivedMillis &&
                _autoPowerEnabledCounter > 0) {
            // We have received a new PowerMeter value. Also we're _autoPowerEnabled
            // So we're good to calculate a new limit

            _lastPowerMeterUpdateReceivedMillis = PowerMeter.getLastUpdate();

            // Calculate new power limit
            float newPowerLimit = -1 * round(PowerMeter.getPowerTotal());
            float efficiency =  (_rp.efficiency > 0.5 ? _rp.efficiency : 1.0);

            // Powerlimit is the requested output power + permissable Grid consumption factoring in the efficiency factor
            newPowerLimit += _rp.output_power + config.Huawei.Auto_Power_Target_Power_Consumption / efficiency;

            if (verboseLogging){
                MessageOutput.printf("[HuaweiCanClass::loop] newPowerLimit: %f, output_power: %f \r\n", newPowerLimit, _rp.output_power);
            }

            // Check whether the battery SoC limit setting is enabled
            if (config.Battery.Enabled && config.Huawei.Auto_Power_BatterySoC_Limits_Enabled) {
                uint8_t _batterySoC = Battery.getStats()->getSoC();
                // Sets power limit to 0 if the BMS reported SoC reaches or exceeds the user configured value
                if (_batterySoC >= config.Huawei.Auto_Power_Stop_BatterySoC_Threshold) {
                    newPowerLimit = 0;
                    if (verboseLogging) {
                        MessageOutput.printf("[HuaweiCanClass::loop] Current battery SoC %i reached "
                                "stop threshold %i, set newPowerLimit to %f \r\n", _batterySoC,
                                config.Huawei.Auto_Power_Stop_BatterySoC_Threshold, newPowerLimit);
                    }
                }
            }

            if (newPowerLimit > config.Huawei.Auto_Power_Lower_Power_Limit) {

                // Check if the output power has dropped below the lower limit (i.e. the battery is full)
                // and if the PSU should be turned off. Also we use a simple counter mechanism here to be able
                // to ramp up from zero output power when starting up
                if (_rp.output_power < config.Huawei.Auto_Power_Lower_Power_Limit) {
                    MessageOutput.printf("[HuaweiCanClass::loop] Power and voltage limit reached. Disabling automatic power control .... \r\n");
                    _autoPowerEnabledCounter--;
                    if (_autoPowerEnabledCounter == 0) {
                        _autoPowerEnabled = false;
                        _setParameter(0.0, Setting::OnlineCurrent);
                        return;
                    }
                } else {
                    _autoPowerEnabledCounter = 10;
                }

                // Limit power to maximum
                if (newPowerLimit > config.Huawei.Auto_Power_Upper_Power_Limit) {
                    newPowerLimit = config.Huawei.Auto_Power_Upper_Power_Limit;
                }

                // Calculate output current
                float calculatedCurrent = efficiency * (newPowerLimit / _rp.output_voltage);

                // Limit output current to value requested by BMS
                float permissableCurrent = stats->getChargeCurrentLimitation() - (stats->getChargeCurrent() - _rp.output_current); // BMS current limit - current from other sources, e.g. Victron MPPT charger
                float outputCurrent = std::min(calculatedCurrent, permissableCurrent);
                outputCurrent= outputCurrent > 0 ? outputCurrent : 0;

                if (verboseLogging) {
                    MessageOutput.printf("[HuaweiCanClass::loop] Setting output current to %.2fA. This is the lower value of calculated %.2fA and BMS permissable %.2fA currents\r\n", outputCurrent, calculatedCurrent, permissableCurrent);
                }
                _autoPowerEnabled = true;
                _setParameter(outputCurrent, Setting::OnlineCurrent);

                // Don't run auto mode some time to allow for output stabilization after issuing a new value
                _autoModeBlockedTillMillis = millis() + 2 * HardwareInterface::DataRequestIntervalMillis;
            } else {
                // requested PL is below minium. Set current to 0
                _autoPowerEnabled = false;
                _setParameter(0.0, Setting::OnlineCurrent);
            }
        }
    }
}

void Controller::setParameter(float val, HardwareInterface::Setting setting)
{
    if (_mode == HUAWEI_MODE_AUTO_INT) { return; }

    _setParameter(val, setting);
}

void Controller::_setParameter(float val, HardwareInterface::Setting setting)
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (!_upHardwareInterface) { return; }

    if (val < 0) {
        MessageOutput.printf("[HuaweiCanClass::_setParameter] Error: Tried to set "
                "voltage/current to negative value %.2f\r\n", val);
        return;
    }

    using Setting = HardwareInterface::Setting;

    // Start PSU if needed
    if (val > HUAWEI_AUTO_MODE_SHUTDOWN_CURRENT &&
            setting == Setting::OnlineCurrent &&
            (_mode == HUAWEI_MODE_AUTO_EXT || _mode == HUAWEI_MODE_AUTO_INT)) {
        enableOutput();
        _outputCurrentOnSinceMillis = millis();
    }

    _upHardwareInterface->setParameter(setting, val);
}

void Controller::setMode(uint8_t mode) {
    std::lock_guard<std::mutex> lock(_mutex);

    if (!_upHardwareInterface) { return; }

    if (mode == HUAWEI_MODE_OFF) {
        disableOutput();
        _mode = HUAWEI_MODE_OFF;
    }
    if (mode == HUAWEI_MODE_ON) {
        enableOutput();
        _mode = HUAWEI_MODE_ON;
    }

    auto const& config = Configuration.get();

    if (mode == HUAWEI_MODE_AUTO_INT && !config.Huawei.Auto_Power_Enabled ) {
        MessageOutput.println("[HuaweiCanClass::setMode] WARNING: Trying to setmode to internal automatic power control without being enabled in the UI. Ignoring command");
        return;
    }

    if (_mode == HUAWEI_MODE_AUTO_INT && mode != HUAWEI_MODE_AUTO_INT) {
        _autoPowerEnabled = false;
        _setParameter(0, HardwareInterface::Setting::OnlineCurrent);
    }

    if (mode == HUAWEI_MODE_AUTO_EXT || mode == HUAWEI_MODE_AUTO_INT) {
        _mode = mode;
    }
}

} // namespace GridCharger::Huawei
