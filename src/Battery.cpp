// SPDX-License-Identifier: GPL-2.0-or-later
#include "Battery.h"
#include "MessageOutput.h"
#include "MqttSettings.h"
#include "PylontechCanReceiver.h"
#include "JkBmsController.h"

BatteryClass Battery;

std::shared_ptr<BatteryStats const> BatteryClass::getStats() const
{
    if (!_upProvider) {
        static auto sspDummyStats = std::make_shared<BatteryStats>();
        return sspDummyStats;
    }

    return _upProvider->getStats();
}

void BatteryClass::init()
{
    CONFIG_T& config = Configuration.get();
    if (!config.Battery_Enabled) { return; }

    bool verboseLogging = config.Battery_VerboseLogging;

    switch (config.Battery_Provider) {
        case 0:
            _upProvider = std::make_unique<PylontechCanReceiver>();
            if (!_upProvider->init(verboseLogging)) { _upProvider = nullptr; }
            break;
        case 1:
            _upProvider = std::make_unique<JkBms::Controller>();
            if (!_upProvider->init(verboseLogging)) { _upProvider = nullptr; }
            break;
        default:
            MessageOutput.printf("Unknown battery provider: %d\r\n", config.Battery_Provider);
            break;
    }
}

void BatteryClass::mqttPublish()
{
    if (!_upProvider) { return; }

    CONFIG_T& config = Configuration.get();

    if (!MqttSettings.getConnected()
            || (millis() - _lastMqttPublish) < (config.Mqtt_PublishInterval * 1000)) {
        return;
    }

    _upProvider->getStats()->mqttPublish();

    _lastMqttPublish = millis();
}

void BatteryClass::loop()
{
    if (!_upProvider) { return; }

    _upProvider->loop();

    mqttPublish();
}

void BatteryClass::reload()
{
    if (_upProvider) {
        _upProvider->deinit();
        _upProvider = nullptr;
    }

    init();
}
