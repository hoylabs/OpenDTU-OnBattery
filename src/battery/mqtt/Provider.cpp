#include <functional>
#include <Configuration.h>
#include <battery/mqtt/Provider.h>
#include <MqttSettings.h>
#include <MessageOutput.h>
#include <Utils.h>

namespace Batteries::Mqtt {

bool Provider::init(bool verboseLogging)
{
    _verboseLogging = verboseLogging;
    _stats->setManufacturer("MQTT");

    auto const& config = Configuration.get();

    _socTopic = config.Battery.Mqtt.SocTopic;
    if (!_socTopic.isEmpty()) {
        MqttSettings.subscribe(_socTopic, 0/*QoS*/,
                std::bind(&Provider::onMqttMessageSoC,
                    this, std::placeholders::_1, std::placeholders::_2,
                    std::placeholders::_3, std::placeholders::_4,
                    std::placeholders::_5, std::placeholders::_6,
                    config.Battery.Mqtt.SocJsonPath)
                );

        if (_verboseLogging) {
            MessageOutput.printf("MqttBattery: Subscribed to '%s' for SoC readings\r\n",
                _socTopic.c_str());
        }
    }

    _voltageTopic = config.Battery.Mqtt.VoltageTopic;
    if (!_voltageTopic.isEmpty()) {
        MqttSettings.subscribe(_voltageTopic, 0/*QoS*/,
                std::bind(&Provider::onMqttMessageVoltage,
                    this, std::placeholders::_1, std::placeholders::_2,
                    std::placeholders::_3, std::placeholders::_4,
                    std::placeholders::_5, std::placeholders::_6,
                    config.Battery.Mqtt.VoltageJsonPath)
                );

        if (_verboseLogging) {
            MessageOutput.printf("MqttBattery: Subscribed to '%s' for voltage readings\r\n",
                _voltageTopic.c_str());
        }
    }

    _currentTopic = config.Battery.Mqtt.CurrentTopic;
    if (!_currentTopic.isEmpty()) {
        MqttSettings.subscribe(_currentTopic, 0/*QoS*/,
                std::bind(&Provider::onMqttMessageCurrent,
                    this, std::placeholders::_1, std::placeholders::_2,
                    std::placeholders::_3, std::placeholders::_4,
                    std::placeholders::_5, std::placeholders::_6,
                    config.Battery.Mqtt.CurrentJsonPath)
                );

        if (_verboseLogging) {
            MessageOutput.printf("MqttBattery: Subscribed to '%s' for current readings\r\n",
                _currentTopic.c_str());
        }
    }

    if (config.Battery.EnableDischargeCurrentLimit && config.Battery.UseBatteryReportedDischargeCurrentLimit) {
        _dischargeCurrentLimitTopic = config.Battery.Mqtt.DischargeCurrentTopic;

        if (!_dischargeCurrentLimitTopic.isEmpty()) {
            MqttSettings.subscribe(_dischargeCurrentLimitTopic, 0/*QoS*/,
                    std::bind(&Provider::onMqttMessageDischargeCurrentLimit,
                        this, std::placeholders::_1, std::placeholders::_2,
                        std::placeholders::_3, std::placeholders::_4,
                        std::placeholders::_5, std::placeholders::_6,
                        config.Battery.Mqtt.DischargeCurrentJsonPath)
                    );

            if (_verboseLogging) {
                MessageOutput.printf("MqttBattery: Subscribed to '%s' for discharge current limit readings\r\n",
                    _dischargeCurrentLimitTopic.c_str());
            }
        }
    }

    return true;
}

void Provider::deinit()
{
    if (!_voltageTopic.isEmpty()) {
        MqttSettings.unsubscribe(_voltageTopic);
    }

    if (!_socTopic.isEmpty()) {
        MqttSettings.unsubscribe(_socTopic);
    }

    if (!_currentTopic.isEmpty()) {
        MqttSettings.unsubscribe(_currentTopic);
    }

    if (!_dischargeCurrentLimitTopic.isEmpty()) {
        MqttSettings.unsubscribe(_dischargeCurrentLimitTopic);
    }
}

void Provider::onMqttMessageSoC(espMqttClientTypes::MessageProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total,
        char const* jsonPath)
{
    auto soc = Utils::getNumericValueFromMqttPayload<float>("MqttBattery",
            std::string(reinterpret_cast<const char*>(payload), len), topic,
            jsonPath);

    if (!soc.has_value()) { return; }

    if (*soc < 0 || *soc > 100) {
        MessageOutput.printf("MqttBattery: Implausible SoC '%.2f' in topic '%s'\r\n",
                *soc, topic);
        return;
    }

    _socPrecision = std::max(_socPrecision, calculatePrecision(*soc));

    _stats->setSoC(*soc, _socPrecision, millis());

    if (_verboseLogging) {
        MessageOutput.printf("MqttBattery: Updated SoC to %.*f from '%s'\r\n",
                _socPrecision, *soc, topic);
    }
}

void Provider::onMqttMessageVoltage(espMqttClientTypes::MessageProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total,
        char const* jsonPath)
{
    auto voltage = Utils::getNumericValueFromMqttPayload<float>("MqttBattery",
            std::string(reinterpret_cast<const char*>(payload), len), topic,
            jsonPath);


    if (!voltage.has_value()) { return; }

    auto const& config = Configuration.get();
    using Unit_t = BatteryVoltageUnit;
    switch (config.Battery.Mqtt.VoltageUnit) {
        case Unit_t::DeciVolts:
            *voltage /= 10;
            break;
        case Unit_t::CentiVolts:
            *voltage /= 100;
            break;
        case Unit_t::MilliVolts:
            *voltage /= 1000;
            break;
        default:
            break;
    }

    // since this project is revolving around Hoymiles microinverters, which can
    // only handle up to 65V of input voltage at best, it is safe to assume that
    // an even higher voltage is implausible.
    if (*voltage < 0 || *voltage > 65) {
        MessageOutput.printf("MqttBattery: Implausible voltage '%.2f' in topic '%s'\r\n",
                *voltage, topic);
        return;
    }

    _stats->setVoltage(*voltage, millis());

    if (_verboseLogging) {
        MessageOutput.printf("MqttBattery: Updated voltage to %.2f from '%s'\r\n",
                *voltage, topic);
    }
}

void Provider::onMqttMessageCurrent(espMqttClientTypes::MessageProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total,
        char const* jsonPath)
{
    auto amperage = Utils::getNumericValueFromMqttPayload<float>("MqttBattery",
            std::string(reinterpret_cast<const char*>(payload), len), topic,
            jsonPath);


    if (!amperage.has_value()) { return; }

    auto const& config = Configuration.get();
    using Unit_t = BatteryAmperageUnit;
    switch (config.Battery.Mqtt.CurrentUnit) {
        case Unit_t::MilliAmps:
            *amperage /= 1000;
            break;
        default:
            break;
    }

    _currentPrecision = std::max(_currentPrecision, calculatePrecision(*amperage));

    _stats->setCurrent(*amperage, _currentPrecision, millis());

    if (_verboseLogging) {
        MessageOutput.printf("MqttBattery: Updated current to %.*f from '%s'\r\n",
                _currentPrecision, *amperage, topic);
    }
}

void Provider::onMqttMessageDischargeCurrentLimit(espMqttClientTypes::MessageProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total,
        char const* jsonPath)
{
    auto amperage = Utils::getNumericValueFromMqttPayload<float>("MqttBattery",
            std::string(reinterpret_cast<const char*>(payload), len), topic,
            jsonPath);


    if (!amperage.has_value()) { return; }

    auto const& config = Configuration.get();
    using Unit_t = BatteryAmperageUnit;
    switch (config.Battery.Mqtt.DischargeCurrentUnit) {
        case Unit_t::MilliAmps:
            *amperage /= 1000;
            break;
        default:
            break;
    }

    if (*amperage < 0) {
        MessageOutput.printf("MqttBattery: Implausible discharge current limit '%.2f' in topic '%s'\r\n",
                *amperage, topic);
        return;
    }

    _stats->setDischargeCurrentLimit(*amperage, millis());

    if (_verboseLogging) {
        MessageOutput.printf("MqttBattery: Updated discharge current limit to %.2f from '%s'\r\n",
                *amperage, topic);
    }
}

uint8_t Provider::calculatePrecision(float value) {
    unsigned factor = 1;
    uint8_t precision = 0;
    while (precision < 2) {
        if (std::floor(value * factor) == value * factor) {
            break;
        }
        ++precision;
        factor *= 10;
    }
    return precision;
}

} // namespace Batteries::Mqtt
