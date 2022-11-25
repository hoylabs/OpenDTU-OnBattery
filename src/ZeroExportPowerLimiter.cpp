// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "ZeroExportPowerLimiter.h"
#include "Configuration.h"
#include "MqttSettings.h"
#include "NetworkSettings.h"
#include <ctime>

ZeroExportPowerLimiterClass ZeroExportPowerLimiter;

void ZeroExportPowerLimiterClass::init()
{
    currentPowerLimit = 0;
}

void ZeroExportPowerLimiterClass::onMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total)
{
    //Serial.print(F("ZeroExportPowerLimiterClass: Received MQTT message on topic: "));
    //Serial.println(topic);

    if (strcmp(topic, TOPIC_CURRENT_POWER_CONSUMPTION_1) == 0) {
        powerMeter1Power = std::stof(std::string((char *)payload, (unsigned int)len));
    }

    if (strcmp(topic, TOPIC_CURRENT_POWER_CONSUMPTION_2) == 0) {
        powerMeter2Power = std::stof(std::string((char *)payload, (unsigned int)len));
    }

    if (strcmp(topic, TOPIC_CURRENT_POWER_CONSUMPTION_3) == 0) {
        powerMeter3Power = std::stof(std::string((char *)payload, (unsigned int)len));
    }

    lastPowerMeterUpdate = millis();
}

void ZeroExportPowerLimiterClass::loop()
{
    if (!MqttSettings.getConnected() || !Hoymiles.getRadio()->isIdle()) {
        return;
    }

    if (millis() - _lastLimitSet < (Configuration.get().ZeroExportPowerLimiter_Interval * 1000)) {
        return;
    }

    auto inverter = Hoymiles.getInverterByPos(0);

    if (inverter == nullptr || !inverter->isProducing()) {
        return;
    }

    // Default value
    // If the power meter values are older than 15 seconds,
    // set the limit to 100W for safety reasons.
    uint16_t defaultPowerLimit = 100;
    uint16_t newPowerLimit = defaultPowerLimit;

    if (millis() - inverter->Statistics()->getLastUpdate() > 1000) {
        return;
    }

    if (millis() - lastPowerMeterUpdate < (30 * 1000)) {
        uint8_t acPower = inverter->Statistics()->getChannelFieldValue(CH0, FLD_PAC);;

        newPowerLimit = int(powerMeter1Power + powerMeter2Power + powerMeter3Power) + currentPowerLimit;
        newPowerLimit = constrain(newPowerLimit, (uint16_t)100, (uint16_t)800);

        Serial.printf("[ZeroExportPowerLimiterClass::loop] powerMeter: %d W acPower: %d W currentPowerLimit: %d\n",
            int(powerMeter1Power + powerMeter2Power + powerMeter3Power), acPower, currentPowerLimit);
    }

    if (abs(currentPowerLimit - newPowerLimit) > 10) {
        Serial.printf("[ZeroExportPowerLimiterClass::loop] Limit Non-Persistent: %d W\n", newPowerLimit);
        inverter->sendActivePowerControlRequest(Hoymiles.getRadio(), newPowerLimit, PowerLimitControlType::AbsolutNonPersistent);
        currentPowerLimit = newPowerLimit;
    } else {
        Serial.printf("[ZeroExportPowerLimiterClass::loop] Diff to old limit < 10, not setting new limit!\n");
    }

    _lastLimitSet = millis();
}
