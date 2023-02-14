// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "PowerMeter.h"
#include "Configuration.h"
#include "MqttSettings.h"
#include "NetworkSettings.h"
#include "SDM.h"
#include <ctime>

PowerMeterClass PowerMeter;

SDM sdm(Serial2, SDM_UART_BAUD, NOT_A_PIN, SERIAL_8N1, SDM_RX_PIN, SDM_TX_PIN);

void PowerMeterClass::init()
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    _powerMeter1Power = 0.0;
    _powerMeter2Power = 0.0;
    _powerMeter3Power = 0.0;
    _powerMeterTotalPower = 0.0;
    _lastPowerMeterUpdate = 0;

    CONFIG_T& config = Configuration.get();

    if(config.PowerMeter_Source == 0){
        if (strlen(config.PowerMeter_MqttTopicPowerMeter1) != 0) {
            MqttSettings.subscribe(config.PowerMeter_MqttTopicPowerMeter1, 0, std::bind(&PowerMeterClass::onMqttMessage, this, _1, _2, _3, _4, _5, _6));
        }

        if (strlen(config.PowerMeter_MqttTopicPowerMeter2) != 0) {
            MqttSettings.subscribe(config.PowerMeter_MqttTopicPowerMeter2, 0, std::bind(&PowerMeterClass::onMqttMessage, this, _1, _2, _3, _4, _5, _6));
        }

        if (strlen(config.PowerMeter_MqttTopicPowerMeter3) != 0) {
            MqttSettings.subscribe(config.PowerMeter_MqttTopicPowerMeter3, 0, std::bind(&PowerMeterClass::onMqttMessage, this, _1, _2, _3, _4, _5, _6));
        }
    }
    if(config.PowerMeter_Source == 1 || config.PowerMeter_Source == 2){
        sdm.begin();
    }

}

void PowerMeterClass::onMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total)
{
    Hoymiles.getMessageOutput()->printf("PowerMeterClass: Received MQTT message on topic: %s\n", topic);

    CONFIG_T& config = Configuration.get();

    if (strcmp(topic, config.PowerMeter_MqttTopicPowerMeter1) == 0) {
        _powerMeter1Power = std::stof(std::string(reinterpret_cast<const char*>(payload), (unsigned int)len));
    }

    if (strcmp(topic, config.PowerMeter_MqttTopicPowerMeter2) == 0) {
        _powerMeter2Power = std::stof(std::string(reinterpret_cast<const char*>(payload), (unsigned int)len));
    }

    if (strcmp(topic, config.PowerMeter_MqttTopicPowerMeter3) == 0) {
        _powerMeter3Power = std::stof(std::string(reinterpret_cast<const char*>(payload), (unsigned int)len));
    }
    
    _powerMeterTotalPower = _powerMeter1Power + _powerMeter2Power + _powerMeter3Power;

    Hoymiles.getMessageOutput()->printf("PowerMeterClass: TotalPower: %5.2f\n", _powerMeterTotalPower);

    _lastPowerMeterUpdate = millis();
}

float PowerMeterClass::getPowerTotal(){
    return _powerMeterTotalPower;
}

void PowerMeterClass::loop()
{
    CONFIG_T& config = Configuration.get();

    if(millis() - _lastPowerMeterUpdate >= (config.PowerMeter_Interval * 1000)){
        _powerMeterTotalPower = static_cast<float>sdm.readVal(SDM_TOTAL_SYSTEM_POWER, 0x01);

        Hoymiles.getMessageOutput()->printf("PowerMeterClass: TotalPower: %5.2f\n", _powerMeterTotalPower);

        _lastPowerMeterUpdate = millis();
        
    }
}
