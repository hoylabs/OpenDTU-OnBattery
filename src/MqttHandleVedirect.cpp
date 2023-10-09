// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Helge Erbe and others
 */
#include "VictronMppt.h"
#include "MqttHandleVedirect.h"
#include "MqttSettings.h"
#include "MessageOutput.h"




MqttHandleVedirectClass MqttHandleVedirect;

// #define MQTTHANDLEVEDIRECT_DEBUG

void MqttHandleVedirectClass::init()
{
    // initially force a full publish
    _nextPublishUpdatesOnly = 0;
    _nextPublishFull = 1;
}

void MqttHandleVedirectClass::loop()
{
    CONFIG_T& config = Configuration.get();

    if (!MqttSettings.getConnected() || !config.Vedirect_Enabled) {
        return;
    }

    if (!VictronMppt.isDataValid()) {
        return;
    }

    if ((millis() >= _nextPublishFull) || (millis() >= _nextPublishUpdatesOnly)) {
        // determine if this cycle should publish full values or updates only
        if (_nextPublishFull <= _nextPublishUpdatesOnly) {
            _PublishFull = true;
        } else {
            _PublishFull = !config.Vedirect_UpdatesOnly;
        }

        #ifdef MQTTHANDLEVEDIRECT_DEBUG
        MessageOutput.printf("\r\n\r\nMqttHandleVedirectClass::loop millis %lu   _nextPublishUpdatesOnly %u   _nextPublishFull %u\r\n", millis(), _nextPublishUpdatesOnly, _nextPublishFull);
        if (_PublishFull) {
            MessageOutput.println("MqttHandleVedirectClass::loop publish full");
        } else {
            MessageOutput.println("MqttHandleVedirectClass::loop publish updates only");
        }
        #endif

        auto const& mpptData = VictronMppt.getData();
        String value;
        String topic = "victron/";
        topic.concat(mpptData.SER);
        topic.concat("/");

        if (_PublishFull || mpptData.PID != _kvFrame.PID)
            MqttSettings.publish(topic + "PID", mpptData.getPidAsString());
        if (_PublishFull || strcmp(mpptData.SER, _kvFrame.SER) != 0)
            MqttSettings.publish(topic + "SER", mpptData.SER );
        if (_PublishFull || strcmp(mpptData.FW, _kvFrame.FW) != 0)
            MqttSettings.publish(topic + "FW", mpptData.FW);
        if (_PublishFull || mpptData.LOAD != _kvFrame.LOAD)
            MqttSettings.publish(topic + "LOAD", mpptData.LOAD == true ? "ON": "OFF");
        if (_PublishFull || mpptData.CS != _kvFrame.CS)
            MqttSettings.publish(topic + "CS", mpptData.getCsAsString());
        if (_PublishFull || mpptData.ERR != _kvFrame.ERR)
            MqttSettings.publish(topic + "ERR", mpptData.getErrAsString());
        if (_PublishFull || mpptData.OR != _kvFrame.OR)
            MqttSettings.publish(topic + "OR", mpptData.getOrAsString());
        if (_PublishFull || mpptData.MPPT != _kvFrame.MPPT)
            MqttSettings.publish(topic + "MPPT", mpptData.getMpptAsString());
        if (_PublishFull || mpptData.HSDS != _kvFrame.HSDS) {
            value = mpptData.HSDS;
            MqttSettings.publish(topic + "HSDS", value);
        }
        if (_PublishFull || mpptData.V != _kvFrame.V) {
            value = mpptData.V;
            MqttSettings.publish(topic + "V", value);
        }
        if (_PublishFull || mpptData.I != _kvFrame.I) {
            value = mpptData.I;
            MqttSettings.publish(topic + "I", value);
        }
        if (_PublishFull || mpptData.P != _kvFrame.P) {
            value = mpptData.P;
            MqttSettings.publish(topic + "P", value);
        }
        if (_PublishFull || mpptData.VPV != _kvFrame.VPV) {
            value = mpptData.VPV;
            MqttSettings.publish(topic + "VPV", value);
        }
        if (_PublishFull || mpptData.IPV != _kvFrame.IPV) {
            value = mpptData.IPV;
            MqttSettings.publish(topic + "IPV", value);
        }
        if (_PublishFull || mpptData.PPV != _kvFrame.PPV) {
            value = mpptData.PPV;
            MqttSettings.publish(topic + "PPV", value);
        }
        if (_PublishFull || mpptData.E != _kvFrame.E) {
            value = mpptData.E;
            MqttSettings.publish(topic + "E", value);
        }
        if (_PublishFull || mpptData.H19 != _kvFrame.H19) {
            value = mpptData.H19;
            MqttSettings.publish(topic + "H19", value);
        }
        if (_PublishFull || mpptData.H20 != _kvFrame.H20) {
            value = mpptData.H20;
            MqttSettings.publish(topic + "H20", value);
        }
        if (_PublishFull || mpptData.H21 != _kvFrame.H21) {
            value = mpptData.H21;
            MqttSettings.publish(topic + "H21", value);
        }
        if (_PublishFull || mpptData.H22 != _kvFrame.H22) {
            value = mpptData.H22;
            MqttSettings.publish(topic + "H22", value);
        }
        if (_PublishFull || mpptData.H23 != _kvFrame.H23) {
            value = mpptData.H23;
            MqttSettings.publish(topic + "H23", value);
        }
        if (!_PublishFull) {
            _kvFrame = mpptData;
        }

        // now calculate next points of time to publish
        _nextPublishUpdatesOnly = millis() + (config.Mqtt_PublishInterval * 1000);

        if (_PublishFull) {
            // when Home Assistant MQTT-Auto-Discovery is active,
            // and "enable expiration" is active, all values must be published at
            // least once before the announced expiry interval is reached
            if ((config.Vedirect_UpdatesOnly) && (config.Mqtt_Hass_Enabled) && (config.Mqtt_Hass_Expire)) {
                _nextPublishFull = millis() + (((config.Mqtt_PublishInterval * 3) - 1) * 1000);

                #ifdef MQTTHANDLEVEDIRECT_DEBUG
                uint32_t _tmpNextFullSeconds = (config.Mqtt_PublishInterval * 3) - 1;
                MessageOutput.printf("MqttHandleVedirectClass::loop _tmpNextFullSeconds %u - _nextPublishFull %u \r\n", _tmpNextFullSeconds, _nextPublishFull);
                #endif

            } else {
                // no future publish full needed
                _nextPublishFull = UINT32_MAX;
            }
        }

        #ifdef MQTTHANDLEVEDIRECT_DEBUG
        MessageOutput.printf("MqttHandleVedirectClass::loop _nextPublishUpdatesOnly %u   _nextPublishFull %u\r\n", _nextPublishUpdatesOnly, _nextPublishFull);
        #endif
    }
}