// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <ArduinoJson.h>
#include "MqttHassPublisher.h"

class MqttHandleBatteryHassClass : public MqttHassPublisher {
public:
    void init(Scheduler& scheduler);
    void forceUpdate() { _doPublish = true; }

private:
    void loop();
    void publishBinarySensor(const char* caption, const char* icon, const char* subTopic, const char* payload_on, const char* payload_off);
    void publishSensor(const char* caption, const char* icon, const char* subTopic, const char* deviceClass = NULL, const char* stateClass = NULL, const char* unitOfMeasurement = NULL);

    void publishBinarySensor2(const char* caption, const char* icon, const char* sensorId, const char* subTopic);
    JsonObject createDeviceInfo();

    String configTopicPrefix();

    Task _loopTask;

    bool _doPublish = true;
    String serial = "0001"; // pseudo-serial, can be replaced in future with real serialnumber
};

extern MqttHandleBatteryHassClass MqttHandleBatteryHass;
