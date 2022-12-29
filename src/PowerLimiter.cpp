// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "PowerLimiter.h"
#include "Configuration.h"
#include "MqttSettings.h"
#include "NetworkSettings.h"
#include <ctime>

ZeroExportPowerLimiterClass ZeroExportPowerLimiter;

void ZeroExportPowerLimiterClass::init()
{
    _lastRequestedPowerLimit = 0;
}

void ZeroExportPowerLimiterClass::onMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total)
{
    Serial.print(F("ZeroExportPowerLimiterClass: Received MQTT message on topic: "));
    Serial.println(topic);

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

void ZeroExportPowerLimiterClass::loop()
{
    CONFIG_T& config = Configuration.get();

    if (!config.PowerLimiter_Enabled
            || !MqttSettings.getConnected()
            || !Hoymiles.getRadio()->isIdle()
            //|| (millis() - _lastCommandSent) < (config.PowerLimiter_Interval * 1000)
            || (millis() - _lastLoop) < (config.PowerLimiter_Interval * 1000)) {
        return;
    }

    _lastLoop = millis();

    auto inverter = Hoymiles.getInverterByPos(0);

    if (inverter == nullptr || !inverter->isReachable()) {
        return;
    }

    float dcVoltage = inverter->Statistics()->getChannelFieldValue(CH1, FLD_UDC);

    if (millis() - _lastPowerMeterUpdate < (30 * 1000)) {
        Serial.printf("[ZeroExportPowerLimiterClass::loop] dcVoltage: %f config.PowerLimiter_VoltageStartThreshold: %f config.PowerLimiter_VoltageStopThreshold: %f inverter->isProducing(): %d\n",
            dcVoltage, config.PowerLimiter_VoltageStartThreshold, config.PowerLimiter_VoltageStopThreshold, inverter->isProducing());
    }

    if (!inverter->isProducing()) {
        if (dcVoltage > 0.0
                && config.PowerLimiter_VoltageStartThreshold > 0.0
                && dcVoltage >= config.PowerLimiter_VoltageStartThreshold) {
            // DC voltage high enough, start the inverter
            Serial.printf("[ZeroExportPowerLimiterClass::loop] Starting up inverter...\n");
            _lastCommandSent = millis();
            inverter->sendPowerControlRequest(Hoymiles.getRadio(), true);
        }

        return;
    } else if (inverter->isProducing()
            && dcVoltage > 0.0
            && config.PowerLimiter_VoltageStopThreshold > 0.0
            && dcVoltage <= config.PowerLimiter_VoltageStopThreshold) {
        // DC voltage too low, stop the inverter
        Serial.printf("[ZeroExportPowerLimiterClass::loop] Stopping inverter...\n");
        _lastCommandSent = millis();
        inverter->sendPowerControlRequest(Hoymiles.getRadio(), false);
        return;
    }

    uint16_t newPowerLimit = 0;

    if (millis() - _lastPowerMeterUpdate < (30 * 1000)) {
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

        Serial.printf("[ZeroExportPowerLimiterClass::loop] powerMeter: %d W lastRequestedPowerLimit: %d\n",
            int(_powerMeter1Power + _powerMeter2Power + _powerMeter3Power), _lastRequestedPowerLimit);
    } else {
        // If the power meter values are older than 30 seconds,
        // set the limit to config.PowerLimiter_LowerPowerLimit for safety reasons.
        newPowerLimit = config.PowerLimiter_LowerPowerLimit;
    }

    //if (abs(currentPowerLimit - newPowerLimit) > 10) {
    Serial.printf("[ZeroExportPowerLimiterClass::loop] Limit Non-Persistent: %d W\n", newPowerLimit);
    inverter->sendActivePowerControlRequest(Hoymiles.getRadio(), newPowerLimit, PowerLimitControlType::AbsolutNonPersistent);
    _lastRequestedPowerLimit = newPowerLimit;
    //} else {
    //    Serial.printf("[ZeroExportPowerLimiterClass::loop] Diff to old limit < 10, not setting new limit!\n");
    //}

    _lastCommandSent = millis();
}
