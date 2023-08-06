// SPDX-License-Identifier: GPL-2.0-or-later
#include "Battery.h"
#include "PinMapping.h"
#include "MessageOutput.h"
#include "MqttSettings.h"
#include "PylontechCanReceiver.h"
#include "JkBmsController.h"

BatteryClass Battery;

void BatteryClass::deinit()
{
    _stats = std::make_shared<BatteryStats>();

    switch (_provider) {
        case Provider::None:
            return;
            break;
        case Provider::Pylontech:
            PylontechCanReceiver.deinit();
            break;
        case Provider::JkBmsUart:
        case Provider::JkBmsTransceiver:
            JkBms::Controller.deinit();
            break;
    }
}

void BatteryClass::init()
{
    CONFIG_T& config = Configuration.get();
    if (!config.Battery_Enabled) {
        _provider = Provider::None;
        return;
    }

    _provider = static_cast<Provider>(config.Battery_Provider);

    const PinMapping_t& pin = PinMapping.get();

    switch (_provider) {
        case Provider::Pylontech:
            MessageOutput.println(F("Initialize Pylontech interface..."));
            if (PinMapping.isValidBatteryConfig()) {
                auto pylontechStats = std::make_shared<PylontechBatteryStats>();
                MessageOutput.printf("Pylontech Battery rx = %d, tx = %d\r\n", pin.battery_rx, pin.battery_tx);
                PylontechCanReceiver.init(pin.battery_rx, pin.battery_tx, pylontechStats);
                _stats = pylontechStats;
                MessageOutput.println(F("done"));
            } else {
                MessageOutput.println(F("Invalid Pylontech pin config"));
            }
            break;
        case Provider::JkBmsUart:
        case Provider::JkBmsTransceiver:
            MessageOutput.println(F("Initialize JK BMS interface..."));
            if (PinMapping.isValidBatteryConfig()) {
                auto jkbmsStats = std::make_shared<JkBmsBatteryStats>();
                MessageOutput.printf("JK BMS rx = %d, rxen = %d, tx = %d, txen = %d\r\n",
                        pin.battery_rx, pin.battery_rxen, pin.battery_tx, pin.battery_txen);
                JkBms::Controller.init(pin.battery_rx, pin.battery_rxen,
                        pin.battery_tx, pin.battery_txen, jkbmsStats);
                _stats = jkbmsStats;
                MessageOutput.println(F("done"));
            } else {
                MessageOutput.println(F("Invalid JK BMS pin config"));
            }
            break;
        default:
            MessageOutput.printf("Unknown battery provider: %d\r\n", config.Battery_Provider);
    }
}

void BatteryClass::mqttPublish()
{
    if (!_stats) { return; }

    CONFIG_T& config = Configuration.get();

    if (!MqttSettings.getConnected()
            || (millis() - _lastMqttPublish) < (config.Mqtt_PublishInterval * 1000)) {
        return;
    }

    _stats->mqttPublish();

    _lastMqttPublish = millis();
}

void BatteryClass::loop()
{
    switch (_provider) {
        case Provider::None:
            return;
            break;
        case Provider::Pylontech:
            PylontechCanReceiver.loop();
            break;
        case Provider::JkBmsUart:
        case Provider::JkBmsTransceiver:
            JkBms::Controller.loop();
            break;
    }

    mqttPublish();
}

void BatteryClass::reload()
{
    deinit();
    init();
}
