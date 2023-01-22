// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "PowerLimiter.h"
#include "Configuration.h"
#include "MqttSettings.h"
#include "NetworkSettings.h"
#include <VeDirectFrameHandler.h>
#include <ctime>

PowerLimiterClass PowerLimiter;

void PowerLimiterClass::init()
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    _lastRequestedPowerLimit = 0;

    CONFIG_T& config = Configuration.get();

    // Zero export power limiter
    if (strlen(config.PowerLimiter_MqttTopicPowerMeter1) != 0) {
        MqttSettings.subscribe(config.PowerLimiter_MqttTopicPowerMeter1, 0, std::bind(&PowerLimiterClass::onMqttMessage, this, _1, _2, _3, _4, _5, _6));
    }

    if (strlen(config.PowerLimiter_MqttTopicPowerMeter2) != 0) {
        MqttSettings.subscribe(config.PowerLimiter_MqttTopicPowerMeter2, 0, std::bind(&PowerLimiterClass::onMqttMessage, this, _1, _2, _3, _4, _5, _6));
    }

    if (strlen(config.PowerLimiter_MqttTopicPowerMeter3) != 0) {
        MqttSettings.subscribe(config.PowerLimiter_MqttTopicPowerMeter3, 0, std::bind(&PowerLimiterClass::onMqttMessage, this, _1, _2, _3, _4, _5, _6));
    }

    _consumeSolarPowerOnly = false;
}

void PowerLimiterClass::onMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total)
{
    Hoymiles.getMessageOutput()->printf("PowerLimiterClass: Received MQTT message on topic: %s\n", topic);

    CONFIG_T& config = Configuration.get();

    if (strcmp(topic, config.PowerLimiter_MqttTopicPowerMeter1) == 0) {
        _powerMeter1Power = std::stof(std::string((char *)payload, (unsigned int)len));
    }

    if (strcmp(topic, config.PowerLimiter_MqttTopicPowerMeter2) == 0) {
        _powerMeter2Power = std::stof(std::string((char *)payload, (unsigned int)len));
    }

    if (strcmp(topic, config.PowerLimiter_MqttTopicPowerMeter3) == 0) {
        _powerMeter3Power = std::stof(std::string((char *)payload, (unsigned int)len));
    }

    _lastPowerMeterUpdate = millis();
}

void PowerLimiterClass::loop()
{
    CONFIG_T& config = Configuration.get();

    if (!config.PowerLimiter_Enabled
            || !MqttSettings.getConnected()
            || !Hoymiles.getRadio()->isIdle()
            || (millis() - _lastCommandSent) < (config.PowerLimiter_Interval * 1000)
            || (millis() - _lastLoop) < (config.PowerLimiter_Interval * 1000)) {
        return;
    }

    _lastLoop = millis();

    auto inverter = Hoymiles.getInverterByPos(0);

    if (inverter == nullptr || !inverter->isReachable()) {
        return;
    }

    float dcVoltage = inverter->Statistics()->getChannelFieldValue(CH1, FLD_UDC);

    if ((millis() - inverter->Statistics()->getLastUpdate()) > 10000
            || dcVoltage <= 0.0) {
        return;
    }

    long victronChargePower = this->getDirectSolarPower();

    Hoymiles.getMessageOutput()->printf("[PowerLimiterClass::loop] victronChargePower: %d\n",
        (int)victronChargePower);

    if (millis() - _lastPowerMeterUpdate < (30 * 1000)) {
        Hoymiles.getMessageOutput()->printf("[PowerLimiterClass::loop] dcVoltage: %f config.PowerLimiter_VoltageStartThreshold: %f config.PowerLimiter_VoltageStopThreshold: %f inverter->isProducing(): %d\n",
            dcVoltage, config.PowerLimiter_VoltageStartThreshold, config.PowerLimiter_VoltageStopThreshold, inverter->isProducing());
    }

    if (inverter->isProducing()) {
        float acPower = inverter->Statistics()->getChannelFieldValue(CH0, FLD_PAC);
        float correctedDcVoltage = dcVoltage + (acPower * config.PowerLimiter_VoltageLoadCorrectionFactor);

        if (_consumeSolarPowerOnly && correctedDcVoltage >= config.PowerLimiter_VoltageStartThreshold) {
            // The battery is full enough again, use Solar power and battery power from now on.
            _consumeSolarPowerOnly = false;
        } else if (!_consumeSolarPowerOnly && correctedDcVoltage <= config.PowerLimiter_VoltageStopThreshold) {
            // The battery voltage dropped too low
            _consumeSolarPowerOnly = true;
        }

        if (config.PowerLimiter_VoltageStopThreshold > 0.0
                && correctedDcVoltage <= config.PowerLimiter_VoltageStopThreshold
                && victronChargePower < 10) {
            // DC voltage too low, stop the inverter
            Hoymiles.getMessageOutput()->printf("[PowerLimiterClass::loop] DC voltage: %f Corrected DC voltage: %f...\n",
                dcVoltage, correctedDcVoltage);
            Hoymiles.getMessageOutput()->println("[PowerLimiterClass::loop] Stopping inverter...\n");
            inverter->sendPowerControlRequest(Hoymiles.getRadio(), false);

            uint16_t newPowerLimit = (uint16_t)config.PowerLimiter_LowerPowerLimit;
            inverter->sendActivePowerControlRequest(Hoymiles.getRadio(), newPowerLimit, PowerLimitControlType::AbsolutNonPersistent);
            _lastRequestedPowerLimit = newPowerLimit;

            _lastCommandSent = millis();
            _consumeSolarPowerOnly = false;

            return;
        }
    } else {
        if ((config.PowerLimiter_VoltageStartThreshold > 0.0
                && dcVoltage >= config.PowerLimiter_VoltageStartThreshold)
                || victronChargePower >= 20) {
            // DC voltage high enough, start the inverter
            Hoymiles.getMessageOutput()->println("[PowerLimiterClass::loop] Starting up inverter...\n");
            _lastCommandSent = millis();
            inverter->sendPowerControlRequest(Hoymiles.getRadio(), true);

            // In this mode, the inverter should consume the current solar power only
            // and not drain additional power from the battery
            if (dcVoltage < config.PowerLimiter_VoltageStartThreshold) {
                _consumeSolarPowerOnly = true;
            }
        }

        return;
    }

    uint16_t newPowerLimit = 0;

    if (_consumeSolarPowerOnly) {
        // Battery voltage too low, use Victron solar power only
        newPowerLimit = victronChargePower;
    } else if (millis() - _lastPowerMeterUpdate < (30 * 1000)) {
        newPowerLimit = int(_powerMeter1Power + _powerMeter2Power + _powerMeter3Power);

        if (config.PowerLimiter_IsInverterBehindPowerMeter) {
            // If the inverter the behind the power meter (part of measurement),
            // the produced power of this inverter has also to be taken into account.
            // We don't use FLD_PAC from the statistics, because that
            // data might be too old and unrelieable.
            newPowerLimit += _lastRequestedPowerLimit;
        }

        newPowerLimit -= 10;

        newPowerLimit = constrain(newPowerLimit, (uint16_t)config.PowerLimiter_LowerPowerLimit, (uint16_t)config.PowerLimiter_UpperPowerLimit);

        Hoymiles.getMessageOutput()->printf("[PowerLimiterClass::loop] powerMeter: %d W lastRequestedPowerLimit: %d\n",
            int(_powerMeter1Power + _powerMeter2Power + _powerMeter3Power), _lastRequestedPowerLimit);
    } else {
        // If the power meter values are older than 30 seconds,
        // set the limit to config.PowerLimiter_LowerPowerLimit for safety reasons.
        newPowerLimit = config.PowerLimiter_LowerPowerLimit;
    }

    Hoymiles.getMessageOutput()->printf("[PowerLimiterClass::loop] Limit Non-Persistent: %d W\n", newPowerLimit);
    inverter->sendActivePowerControlRequest(Hoymiles.getRadio(), newPowerLimit, PowerLimitControlType::AbsolutNonPersistent);
    _lastRequestedPowerLimit = newPowerLimit;

    _lastCommandSent = millis();
}

bool PowerLimiterClass::canUseDirectSolarPower() {
    if (!Configuration.get().Vedirect_Enabled
            || !VeDirect.veMap.count("PPV")) {
        return false;
    }

    if (VeDirect.veMap["PPV"].toInt() < 10) {
        // Not enough power
        return false;
    }

    return true;
}

long PowerLimiterClass::getDirectSolarPower() {
    if (!this->canUseDirectSolarPower()) {
        return 0;
    }

    return VeDirect.veMap["PPV"].toInt();
}
