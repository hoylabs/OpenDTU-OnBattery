// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "MqttHandleHuawei.h"
#include "MessageOutput.h"
#include "MqttSettings.h"
#include <gridcharger/huawei/Controller.h>
#include "WebApi_Huawei.h"
#include <ctime>

MqttHandleHuaweiClass MqttHandleHuawei;

void MqttHandleHuaweiClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&MqttHandleHuaweiClass::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.enable();

    subscribeTopics();

    _lastPublish = millis();
}

void MqttHandleHuaweiClass::forceUpdate()
{
    _lastPublish = 0;
}

void MqttHandleHuaweiClass::subscribeTopics()
{
    String const& prefix = MqttSettings.getPrefix();

    auto subscribe = [&prefix, this](char const* subTopic, Topic t) {
        String fullTopic(prefix + _cmdtopic.data() + subTopic);
        MqttSettings.subscribe(fullTopic.c_str(), 0,
                std::bind(&MqttHandleHuaweiClass::onMqttMessage, this, t,
                    std::placeholders::_1, std::placeholders::_2,
                    std::placeholders::_3, std::placeholders::_4,
                    std::placeholders::_5, std::placeholders::_6));
    };

    for (auto const& s : _subscriptions) {
        subscribe(s.first.data(), s.second);
    }
}

void MqttHandleHuaweiClass::unsubscribeTopics()
{
    String const prefix = MqttSettings.getPrefix() + _cmdtopic.data();
    for (auto const& s : _subscriptions) {
        MqttSettings.unsubscribe(prefix + s.first.data());
    }
}

void MqttHandleHuaweiClass::loop()
{
    const CONFIG_T& config = Configuration.get();

    std::unique_lock<std::mutex> mqttLock(_mqttMutex);

    if (!config.Huawei.Enabled) {
        _mqttCallbacks.clear();
        return;
    }

    for (auto& callback : _mqttCallbacks) { callback(); }
    _mqttCallbacks.clear();

    mqttLock.unlock();

    if (!MqttSettings.getConnected() ) {
        return;
    }

    if ((millis() - _lastPublish) <= (config.Mqtt.PublishInterval * 1000)) {
        return;
    }

    auto const& dataPoints = HuaweiCan.getDataPoints();

#define PUB(l, t) \
    { \
        auto oDataPoint = dataPoints.get<GridCharger::Huawei::DataPointLabel::l>(); \
        if (oDataPoint) { \
            MqttSettings.publish("huawei/" t, String(*oDataPoint)); \
        } \
    }

    PUB(InputVoltage, "input_voltage");
    PUB(InputCurrent, "input_current");
    PUB(InputPower, "input_power");
    PUB(OutputVoltage, "output_voltage");
    PUB(OutputCurrent, "output_current");
    PUB(OutputCurrentMax, "max_output_current");
    PUB(OutputPower, "output_power");
    PUB(InputTemperature, "input_temp");
    PUB(OutputTemperature, "output_temp");
    PUB(Efficiency, "efficiency");
#undef PUB

    MqttSettings.publish("huawei/data_age", String((millis() - dataPoints.getLastUpdate()) / 1000));
    MqttSettings.publish("huawei/mode", String(HuaweiCan.getMode()));

    _lastPublish = millis();
}


void MqttHandleHuaweiClass::onMqttMessage(Topic t,
        const espMqttClientTypes::MessageProperties& properties,
        const char* topic, const uint8_t* payload, size_t len,
        size_t index, size_t total)
{
    std::string strValue(reinterpret_cast<const char*>(payload), len);
    float payload_val = -1;
    try {
        payload_val = std::stof(strValue);
    }
    catch (std::invalid_argument const& e) {
        MessageOutput.printf("Huawei MQTT handler: cannot parse payload of topic '%s' as float: %s\r\n",
                topic, strValue.c_str());
        return;
    }

    std::lock_guard<std::mutex> mqttLock(_mqttMutex);
    using Setting = GridCharger::Huawei::HardwareInterface::Setting;

    switch (t) {
        case Topic::LimitOnlineVoltage:
            MessageOutput.printf("Limit Voltage: %f V\r\n", payload_val);
            _mqttCallbacks.push_back(std::bind(&GridCharger::Huawei::Controller::setParameter,
                        &HuaweiCan, payload_val, Setting::OnlineVoltage));
            break;

        case Topic::LimitOfflineVoltage:
            MessageOutput.printf("Offline Limit Voltage: %f V\r\n", payload_val);
            _mqttCallbacks.push_back(std::bind(&GridCharger::Huawei::Controller::setParameter,
                        &HuaweiCan, payload_val, Setting::OfflineVoltage));
            break;

        case Topic::LimitOnlineCurrent:
            MessageOutput.printf("Limit Current: %f A\r\n", payload_val);
            _mqttCallbacks.push_back(std::bind(&GridCharger::Huawei::Controller::setParameter,
                        &HuaweiCan, payload_val, Setting::OnlineCurrent));
            break;

        case Topic::LimitOfflineCurrent:
            MessageOutput.printf("Offline Limit Current: %f A\r\n", payload_val);
            _mqttCallbacks.push_back(std::bind(&GridCharger::Huawei::Controller::setParameter,
                        &HuaweiCan, payload_val, Setting::OfflineCurrent));
            break;

        case Topic::Mode:
            switch (static_cast<int>(payload_val)) {
                case 3:
                    MessageOutput.println("[Huawei MQTT::] Received MQTT msg. New mode: Full internal control");
                    _mqttCallbacks.push_back(std::bind(&GridCharger::Huawei::Controller::setMode,
                                &HuaweiCan, HUAWEI_MODE_AUTO_INT));
                    break;

                case 2:
                    MessageOutput.println("[Huawei MQTT::] Received MQTT msg. New mode: Internal on/off control, external power limit");
                    _mqttCallbacks.push_back(std::bind(&GridCharger::Huawei::Controller::setMode,
                                &HuaweiCan, HUAWEI_MODE_AUTO_EXT));
                    break;

                case 1:
                    MessageOutput.println("[Huawei MQTT::] Received MQTT msg. New mode: Turned ON");
                    _mqttCallbacks.push_back(std::bind(&GridCharger::Huawei::Controller::setMode,
                                &HuaweiCan, HUAWEI_MODE_ON));
                    break;

                case 0:
                    MessageOutput.println("[Huawei MQTT::] Received MQTT msg. New mode: Turned OFF");
                    _mqttCallbacks.push_back(std::bind(&GridCharger::Huawei::Controller::setMode,
                                &HuaweiCan, HUAWEI_MODE_OFF));
                    break;

                default:
                    MessageOutput.printf("[Huawei MQTT::] Invalid mode %.0f\r\n", payload_val);
                    break;
            }
            break;
    }
}
