// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler, Malte Schmidt and others
 */
#include "MqttSettings.h"
#include "MqttHandlePowerLimiter.h"
#include "PowerLimiter.h"
#include <ctime>
#include <string>
#include <LogHelper.h>

static const char* TAG = "dynamicPowerLimiter";
static const char* SUBTAG = "MQTT";

MqttHandlePowerLimiterClass MqttHandlePowerLimiter;

void MqttHandlePowerLimiterClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&MqttHandlePowerLimiterClass::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.enable();

    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;

    subscribeTopics();

    _lastPublish = millis();
}

void MqttHandlePowerLimiterClass::forceUpdate()
{
    _lastPublish = 0;
}

void MqttHandlePowerLimiterClass::subscribeTopics()
{
    String const& prefix = MqttSettings.getPrefix();

    auto subscribe = [&prefix, this](char const* subTopic, MqttPowerLimiterCommand command) {
        String fullTopic(prefix + _cmdtopic.data() + subTopic);
        MqttSettings.subscribe(fullTopic.c_str(), 0,
                std::bind(&MqttHandlePowerLimiterClass::onMqttCmd, this, command,
                    std::placeholders::_1, std::placeholders::_2,
                    std::placeholders::_3, std::placeholders::_4));
    };

    for (auto const& s : _subscriptions) {
        subscribe(s.first.data(), s.second);
    }
}

void MqttHandlePowerLimiterClass::unsubscribeTopics()
{
    String const prefix = MqttSettings.getPrefix() + _cmdtopic.data();
    for (auto const& s : _subscriptions) {
        MqttSettings.unsubscribe(prefix + s.first.data());
    }
}

void MqttHandlePowerLimiterClass::loop()
{
    std::unique_lock<std::mutex> mqttLock(_mqttMutex);

    auto const& config = Configuration.get();

    if (!config.PowerLimiter.Enabled) {
        _mqttCallbacks.clear();
        return;
    }

    for (auto& callback : _mqttCallbacks) { callback(); }
    _mqttCallbacks.clear();

    mqttLock.unlock();

    if (!MqttSettings.getConnected() ) { return; }

    if ((millis() - _lastPublish) < (config.Mqtt.PublishInterval * 1000)) {
        return;
    }

    _lastPublish = millis();

    auto val = static_cast<unsigned>(PowerLimiter.getMode());
    MqttSettings.publish("powerlimiter/status/mode", String(val));

    MqttSettings.publish("powerlimiter/status/upper_power_limit", String(config.PowerLimiter.TotalUpperPowerLimit));

    MqttSettings.publish("powerlimiter/status/target_power_consumption", String(config.PowerLimiter.TargetPowerConsumption));

    MqttSettings.publish("powerlimiter/status/inverter_update_timeouts", String(PowerLimiter.getInverterUpdateTimeouts()));

    // no thresholds are relevant for setups without a battery
    if (!PowerLimiter.usesBatteryPoweredInverter()) { return; }

    MqttSettings.publish("powerlimiter/status/threshold/voltage/start", String(config.PowerLimiter.VoltageStartThreshold));
    MqttSettings.publish("powerlimiter/status/threshold/voltage/stop", String(config.PowerLimiter.VoltageStopThreshold));

    if (config.SolarCharger.Enabled) {
        MqttSettings.publish("powerlimiter/status/full_solar_passthrough_active", String(PowerLimiter.isFullSolarPassthroughActive()));
        MqttSettings.publish("powerlimiter/status/threshold/voltage/full_solar_passthrough_start", String(config.PowerLimiter.FullSolarPassThroughStartVoltage));
        MqttSettings.publish("powerlimiter/status/threshold/voltage/full_solar_passthrough_stop", String(config.PowerLimiter.FullSolarPassThroughStopVoltage));
    }

    if (!config.Battery.Enabled || config.PowerLimiter.IgnoreSoc) { return; }

    MqttSettings.publish("powerlimiter/status/threshold/soc/start", String(config.PowerLimiter.BatterySocStartThreshold));
    MqttSettings.publish("powerlimiter/status/threshold/soc/stop", String(config.PowerLimiter.BatterySocStopThreshold));

    if (config.SolarCharger.Enabled) {
        MqttSettings.publish("powerlimiter/status/threshold/soc/full_solar_passthrough", String(config.PowerLimiter.FullSolarPassThroughSoc));
    }
}

void MqttHandlePowerLimiterClass::onMqttCmd(MqttPowerLimiterCommand command, const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len)
{
    std::string strValue(reinterpret_cast<const char*>(payload), len);
    float payload_val = -1;
    try {
        payload_val = std::stof(strValue);
    }
    catch (std::invalid_argument const& e) {
        DTU_LOGE("cannot parse payload of topic '%s' as float: %s", topic, strValue.c_str());
        return;
    }
    const int intValue = static_cast<int>(payload_val);

    if (command == MqttPowerLimiterCommand::Mode) {
        std::lock_guard<std::mutex> mqttLock(_mqttMutex);
        using Mode = PowerLimiterClass::Mode;
        Mode mode = static_cast<Mode>(intValue);
        if (mode == Mode::UnconditionalFullSolarPassthrough) {
            DTU_LOGI("Power limiter unconditional full solar PT");
            _mqttCallbacks.push_back(std::bind(&PowerLimiterClass::setMode,
                        &PowerLimiter, Mode::UnconditionalFullSolarPassthrough));
        } else if (mode == Mode::Disabled) {
            DTU_LOGI("Power limiter disabled (override)");
            _mqttCallbacks.push_back(std::bind(&PowerLimiterClass::setMode,
                        &PowerLimiter, Mode::Disabled));
        } else if (mode == Mode::Normal) {
            DTU_LOGI("Power limiter normal operation");
            _mqttCallbacks.push_back(std::bind(&PowerLimiterClass::setMode,
                        &PowerLimiter, Mode::Normal));
        } else {
            DTU_LOGE("PowerLimiter - unknown mode %d", intValue);
        }
        return;
    }

    auto guard = Configuration.getWriteGuard();
    auto& config = guard.getConfig();

    switch (command) {
        case MqttPowerLimiterCommand::Mode:
            // handled separately above to avoid locking two mutexes
            break;
        case MqttPowerLimiterCommand::BatterySoCStartThreshold:
            if (config.PowerLimiter.BatterySocStartThreshold == intValue) { return; }
            DTU_LOGI("Setting battery SoC start threshold to: %d %%", intValue);
            config.PowerLimiter.BatterySocStartThreshold = intValue;
            break;
        case MqttPowerLimiterCommand::BatterySoCStopThreshold:
            if (config.PowerLimiter.BatterySocStopThreshold == intValue) { return; }
            DTU_LOGI("Setting battery SoC stop threshold to: %d %%", intValue);
            config.PowerLimiter.BatterySocStopThreshold = intValue;
            break;
        case MqttPowerLimiterCommand::FullSolarPassthroughSoC:
            if (config.PowerLimiter.FullSolarPassThroughSoc == intValue) { return; }
            DTU_LOGI("Setting full solar passthrough SoC to: %d %%", intValue);
            config.PowerLimiter.FullSolarPassThroughSoc = intValue;
            break;
        case MqttPowerLimiterCommand::VoltageStartThreshold:
            if (config.PowerLimiter.VoltageStartThreshold == payload_val) { return; }
            DTU_LOGI("Setting voltage start threshold to: %.2f V", payload_val);
            config.PowerLimiter.VoltageStartThreshold = payload_val;
            break;
        case MqttPowerLimiterCommand::VoltageStopThreshold:
            if (config.PowerLimiter.VoltageStopThreshold == payload_val) { return; }
            DTU_LOGI("Setting voltage stop threshold to: %.2f V", payload_val);
            config.PowerLimiter.VoltageStopThreshold = payload_val;
            break;
        case MqttPowerLimiterCommand::FullSolarPassThroughStartVoltage:
            if (config.PowerLimiter.FullSolarPassThroughStartVoltage == payload_val) { return; }
            DTU_LOGI("Setting full solar passthrough start voltage to: %.2f V", payload_val);
            config.PowerLimiter.FullSolarPassThroughStartVoltage = payload_val;
            break;
        case MqttPowerLimiterCommand::FullSolarPassThroughStopVoltage:
            if (config.PowerLimiter.FullSolarPassThroughStopVoltage == payload_val) { return; }
            DTU_LOGI("Setting full solar passthrough stop voltage to: %.2f V", payload_val);
            config.PowerLimiter.FullSolarPassThroughStopVoltage = payload_val;
            break;
        case MqttPowerLimiterCommand::UpperPowerLimit:
            if (config.PowerLimiter.TotalUpperPowerLimit == intValue) { return; }
            DTU_LOGI("Setting total upper power limit to: %d W", intValue);
            config.PowerLimiter.TotalUpperPowerLimit = intValue;
            break;
        case MqttPowerLimiterCommand::TargetPowerConsumption:
            if (config.PowerLimiter.TargetPowerConsumption == intValue) { return; }
            DTU_LOGI("Setting target power consumption to: %d W", intValue);
            config.PowerLimiter.TargetPowerConsumption = intValue;
            break;
    }

    // not reached if the value did not change
    Configuration.write();
}
