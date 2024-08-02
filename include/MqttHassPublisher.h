// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2024 Thomas Basler and others
 */
#pragma once

#include <ArduinoJson.h>

class MqttHassPublisher {
public:
    static void publish(const String& subtopic, const JsonDocument& payload);

protected:
    static JsonObject createDeviceInfo(const String& name, const String& identifiers, const String& model, const String& sw_version, const bool& via_dtu);
    static String getDtuUniqueId();

    void publishBinarySensor(const String& unique_dentifier, const String& name, const String& icon, const String& sensorId, const String& statSubTopic);

    virtual String configTopicPrefix();
    virtual JsonObject createDeviceInfo();

private:
    static String getDtuUrl();
};
