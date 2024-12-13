// SPDX-License-Identifier: GPL-2.0-or-later
#include <Configuration.h>
#include <MqttSettings.h>
#include <MessageOutput.h>
#include <solarcharger/Controller.h>
#include <solarcharger/victron/Stats.h>

namespace SolarChargers::Victron {

void Stats::mqttPublish() const
{
    if ((millis() >= _nextPublishFull) || (millis() >= _nextPublishUpdatesOnly)) {
        auto const& config = Configuration.get();

        // determine if this cycle should publish full values or updates only
        if (_nextPublishFull <= _nextPublishUpdatesOnly) {
            _PublishFull = true;
        } else {
            _PublishFull = !config.SolarCharger.PublishUpdatesOnly;
        }

        for (int idx = 0; idx < SolarCharger.controllerAmount(); ++idx) {
            std::optional<VeDirectMpptController::data_t> optMpptData = SolarCharger.getData(idx);
            if (!optMpptData.has_value()) { continue; }

            auto const& kvFrame = _kvFrames[optMpptData->serialNr_SER];
            publish_mppt_data(*optMpptData, kvFrame);
            if (!_PublishFull) {
                _kvFrames[optMpptData->serialNr_SER] = *optMpptData;
            }
        }

        // now calculate next points of time to publish
        _nextPublishUpdatesOnly = millis() + ::SolarChargers::Stats::getMqttFullPublishIntervalMs();

        if (_PublishFull) {
            // when Home Assistant MQTT-Auto-Discovery is active,
            // and "enable expiration" is active, all values must be published at
            // least once before the announced expiry interval is reached
            if ((config.SolarCharger.PublishUpdatesOnly) && (config.Mqtt.Hass.Enabled) && (config.Mqtt.Hass.Expire)) {
                _nextPublishFull = millis() + (((config.Mqtt.PublishInterval * 3) - 1) * 1000);

            } else {
                // no future publish full needed
                _nextPublishFull = UINT32_MAX;
            }
        }
    }
}

void Stats::publish_mppt_data(const VeDirectMpptController::data_t &currentData,
                                                const VeDirectMpptController::data_t &previousData) const {
    String value;
    String topic = "victron/";
    topic.concat(currentData.serialNr_SER);
    topic.concat("/");

#define PUBLISH(sm, t, val) \
    if (_PublishFull || currentData.sm != previousData.sm) { \
        MqttSettings.publish(topic + t, String(val)); \
    }

    PUBLISH(productID_PID,           "PID",  currentData.getPidAsString().data());
    PUBLISH(serialNr_SER,            "SER",  currentData.serialNr_SER);
    PUBLISH(firmwareVer_FW,          "FWI",  currentData.getFwVersionAsInteger());
    PUBLISH(firmwareVer_FW,          "FWF",  currentData.getFwVersionFormatted());
    PUBLISH(firmwareVer_FW,           "FW",  currentData.firmwareVer_FW);
    PUBLISH(firmwareVer_FWE,         "FWE",  currentData.firmwareVer_FWE);
    PUBLISH(currentState_CS,          "CS",  currentData.getCsAsString().data());
    PUBLISH(errorCode_ERR,           "ERR",  currentData.getErrAsString().data());
    PUBLISH(offReason_OR,             "OR",  currentData.getOrAsString().data());
    PUBLISH(stateOfTracker_MPPT,    "MPPT",  currentData.getMpptAsString().data());
    PUBLISH(daySequenceNr_HSDS,     "HSDS",  currentData.daySequenceNr_HSDS);
    PUBLISH(batteryVoltage_V_mV,       "V",  currentData.batteryVoltage_V_mV / 1000.0);
    PUBLISH(batteryCurrent_I_mA,       "I",  currentData.batteryCurrent_I_mA / 1000.0);
    PUBLISH(batteryOutputPower_W,      "P",  currentData.batteryOutputPower_W);
    PUBLISH(panelVoltage_VPV_mV,     "VPV",  currentData.panelVoltage_VPV_mV / 1000.0);
    PUBLISH(panelCurrent_mA,         "IPV",  currentData.panelCurrent_mA / 1000.0);
    PUBLISH(panelPower_PPV_W,        "PPV",  currentData.panelPower_PPV_W);
    PUBLISH(mpptEfficiency_Percent,    "E",  currentData.mpptEfficiency_Percent);
    PUBLISH(yieldTotal_H19_Wh,       "H19",  currentData.yieldTotal_H19_Wh / 1000.0);
    PUBLISH(yieldToday_H20_Wh,       "H20",  currentData.yieldToday_H20_Wh / 1000.0);
    PUBLISH(maxPowerToday_H21_W,     "H21",  currentData.maxPowerToday_H21_W);
    PUBLISH(yieldYesterday_H22_Wh,   "H22",  currentData.yieldYesterday_H22_Wh / 1000.0);
    PUBLISH(maxPowerYesterday_H23_W, "H23",  currentData.maxPowerYesterday_H23_W);
#undef PUBLILSH

#define PUBLISH_OPT(sm, t, val) \
    if (currentData.sm.first != 0 && (_PublishFull || currentData.sm.second != previousData.sm.second)) { \
        MqttSettings.publish(topic + t, String(val)); \
    }

    PUBLISH_OPT(relayState_RELAY,                         "RELAY",                        currentData.relayState_RELAY.second ? "ON" : "OFF");
    PUBLISH_OPT(loadOutputState_LOAD,                     "LOAD",                         currentData.loadOutputState_LOAD.second ? "ON" : "OFF");
    PUBLISH_OPT(loadCurrent_IL_mA,                        "IL",                           currentData.loadCurrent_IL_mA.second / 1000.0);
    PUBLISH_OPT(NetworkTotalDcInputPowerMilliWatts,       "NetworkTotalDcInputPower",     currentData.NetworkTotalDcInputPowerMilliWatts.second / 1000.0);
    PUBLISH_OPT(MpptTemperatureMilliCelsius,              "MpptTemperature",              currentData.MpptTemperatureMilliCelsius.second / 1000.0);
    PUBLISH_OPT(BatteryAbsorptionMilliVolt,               "BatteryAbsorption",            currentData.BatteryAbsorptionMilliVolt.second / 1000.0);
    PUBLISH_OPT(BatteryFloatMilliVolt,                    "BatteryFloat",                 currentData.BatteryFloatMilliVolt.second / 1000.0);
    PUBLISH_OPT(SmartBatterySenseTemperatureMilliCelsius, "SmartBatterySenseTemperature", currentData.SmartBatterySenseTemperatureMilliCelsius.second / 1000.0);
#undef PUBLILSH_OPT
}

}; // namespace SolarChargers::Victron
