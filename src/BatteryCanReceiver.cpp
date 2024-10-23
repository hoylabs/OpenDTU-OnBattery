// SPDX-License-Identifier: GPL-2.0-or-later
#include "Configuration.h"
#include "BatteryCanReceiver.h"
#include "MqttSettings.h"
#include "MessageOutput.h"
#include "PinMapping.h"
#include <driver/twai.h>

bool BatteryCanReceiver::init(bool verboseLogging, char const* providerName)
{
    _verboseLogging = verboseLogging;
    _providerName = providerName;

    MessageOutput.printf("[%s] Initialize interface...\r\n",
            _providerName);

    auto const& config = Configuration.get();
    _canTopic = config.Battery.MqttCANTopic;
    _canInterface = static_cast<enum CanInterface>(config.Battery.CanInterface);
    if (_canInterface == kMqtt) {
        MqttSettings.subscribe(_canTopic, 0/*QoS*/,
                std::bind(&BatteryCanReceiver::onMqttMessageCAN,
                    this, std::placeholders::_1, std::placeholders::_2,
                    std::placeholders::_3, std::placeholders::_4,
                    std::placeholders::_5, std::placeholders::_6)
                );

        if (_verboseLogging) {
            MessageOutput.printf("BatteryCanReceiver: Subscribed to '%s' for CAN messages\r\n",
                _canTopic.c_str());
        }
        return true;
    }

    const PinMapping_t& pin = PinMapping.get();
    MessageOutput.printf("[%s] Interface rx = %d, tx = %d\r\n",
            _providerName, pin.battery_rx, pin.battery_tx);

    if (pin.battery_rx < 0 || pin.battery_tx < 0) {
        MessageOutput.printf("[%s] Invalid pin config\r\n",
                _providerName);
        return false;
    }

    auto tx = static_cast<gpio_num_t>(pin.battery_tx);
    auto rx = static_cast<gpio_num_t>(pin.battery_rx);
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(tx, rx, TWAI_MODE_NORMAL);

    // interrupts at level 1 are in high demand, at least on ESP32-S3 boards,
    // but only a limited amount can be allocated. failing to allocate an
    // interrupt in the TWAI driver will cause a bootloop. we therefore
    // register the TWAI driver's interrupt at level 2. level 2 interrupts
    // should be available -- we don't really know. we would love to have the
    // esp_intr_dump() function, but that's not available yet in our version
    // of the underlying esp-idf.
    g_config.intr_flags = ESP_INTR_FLAG_LEVEL2;

    // Initialize configuration structures using macro initializers
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Install TWAI driver
    esp_err_t twaiLastResult = twai_driver_install(&g_config, &t_config, &f_config);
    switch (twaiLastResult) {
        case ESP_OK:
            MessageOutput.printf("[%s] Twai driver installed\r\n",
                    _providerName);
            break;
        case ESP_ERR_INVALID_ARG:
            MessageOutput.printf("[%s] Twai driver install - invalid arg\r\n",
                    _providerName);
            return false;
            break;
        case ESP_ERR_NO_MEM:
            MessageOutput.printf("[%s] Twai driver install - no memory\r\n",
                    _providerName);
            return false;
            break;
        case ESP_ERR_INVALID_STATE:
            MessageOutput.printf("[%s] Twai driver install - invalid state\r\n",
                    _providerName);
            return false;
            break;
    }

    // Start TWAI driver
    twaiLastResult = twai_start();
    switch (twaiLastResult) {
        case ESP_OK:
            MessageOutput.printf("[%s] Twai driver started\r\n",
                    _providerName);
            break;
        case ESP_ERR_INVALID_STATE:
            MessageOutput.printf("[%s] Twai driver start - invalid state\r\n",
                    _providerName);
            return false;
            break;
    }

    return true;
}

void BatteryCanReceiver::deinit()
{
    if (_canInterface == kMqtt) {
        MqttSettings.unsubscribe(_canTopic);
        return;
    }

    // Stop TWAI driver
    esp_err_t twaiLastResult = twai_stop();
    switch (twaiLastResult) {
        case ESP_OK:
            MessageOutput.printf("[%s] Twai driver stopped\r\n",
                    _providerName);
            break;
        case ESP_ERR_INVALID_STATE:
            MessageOutput.printf("[%s] Twai driver stop - invalid state\r\n",
                    _providerName);
            break;
    }

    // Uninstall TWAI driver
    twaiLastResult = twai_driver_uninstall();
    switch (twaiLastResult) {
        case ESP_OK:
            MessageOutput.printf("[%s] Twai driver uninstalled\r\n",
                    _providerName);
            break;
        case ESP_ERR_INVALID_STATE:
            MessageOutput.printf("[%s] Twai driver uninstall - invalid state\r\n",
                    _providerName);
            break;
    }
}

void BatteryCanReceiver::loop()
{
    if (_canInterface == kMqtt) {
        return;  // Mqtt CAN messages are event-driven
    }

    // Check for messages. twai_receive is blocking when there is no data so we return if there are no frames in the buffer
    twai_status_info_t status_info;
    esp_err_t twaiLastResult = twai_get_status_info(&status_info);
    if (twaiLastResult != ESP_OK) {
        switch (twaiLastResult) {
            case ESP_ERR_INVALID_ARG:
                MessageOutput.printf("[%s] Twai driver get status - invalid arg\r\n",
                        _providerName);
                break;
            case ESP_ERR_INVALID_STATE:
                MessageOutput.printf("[%s] Twai driver get status - invalid state\r\n",
                        _providerName);
                break;
        }
        return;
    }
    if (status_info.msgs_to_rx == 0) {
        return;
    }

    // Wait for message to be received, function is blocking
    twai_message_t rx_message;
    if (twai_receive(&rx_message, pdMS_TO_TICKS(100)) != ESP_OK) {
        MessageOutput.printf("[%s] Failed to receive message",
                _providerName);
        return;
    }

    postMessage(std::move(rx_message));
}


void BatteryCanReceiver::onMqttMessageCAN(espMqttClientTypes::MessageProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len, size_t index, size_t total)
{
    std::string value(reinterpret_cast<const char*>(payload), len);
    JsonDocument json;

    auto log = [this, topic](char const* format, auto&&... args) -> void {
        MessageOutput.printf("[%s] Topic '%s': ", _providerName, topic);
        MessageOutput.printf(format, args...);
        MessageOutput.println();
    };

    const DeserializationError error = deserializeJson(json, value);
    if (error) {
        log("cannot parse payload '%s' as JSON", value.c_str());
        return;
    }

    if (json.overflowed()) {
        log("payload too large to process as JSON");
        return;
    }

    int canID = json["id"] | -1;
    if (canID == -1) {
        log("JSON is missing message id");
        return;
    }

    twai_message_t rx_message = {};
    rx_message.identifier = canID;
    int maxLen = sizeof(rx_message.data);

    JsonVariant canData = json["data"];
    if (canData.isNull()) {
        log("JSON is missing message data");
        return;
    }

    if (canData.is<char const*>()) {
      String strData = canData.as<String>();
      int len = strData.length();
      if (len > maxLen) {
          log("JSON data has more than %d elements", maxLen);
          return;
      }

      rx_message.data_length_code = len;
      for (int i = 0; i < len; i++) {
        rx_message.data[i] = strData[i];
      }
    } else {
      JsonArray arrayData = canData.as<JsonArray>();
      int len = arrayData.size();
      if (len > maxLen) {
          log("JSON data has more than %d elements", maxLen);
          return;
      }

      rx_message.data_length_code = len;
      for (int i = 0; i < len; i++) {
        rx_message.data[i] = arrayData[i];
      }
    }

    postMessage(std::move(rx_message));
}

void BatteryCanReceiver::postMessage(twai_message_t&& rx_message)
{
    if (_verboseLogging) {
        MessageOutput.printf("[%s] Received CAN message: 0x%04X -",
                _providerName, rx_message.identifier);

        for (int i = 0; i < rx_message.data_length_code; i++) {
            MessageOutput.printf(" %02X", rx_message.data[i]);
        }

        MessageOutput.printf("\r\n");
    }

    onMessage(rx_message);
}

uint8_t BatteryCanReceiver::readUnsignedInt8(uint8_t *data)
{
    return data[0];
}

uint16_t BatteryCanReceiver::readUnsignedInt16(uint8_t *data)
{
    return (data[1] << 8) | data[0];
}

int16_t BatteryCanReceiver::readSignedInt16(uint8_t *data)
{
    return this->readUnsignedInt16(data);
}

int32_t BatteryCanReceiver::readSignedInt24(uint8_t *data)
{
    return (data[2] << 16) | (data[1] << 8) | data[0];
}

uint32_t BatteryCanReceiver::readUnsignedInt32(uint8_t *data)
{
    return (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
}

float BatteryCanReceiver::scaleValue(int32_t value, float factor)
{
    return value * factor;
}

bool BatteryCanReceiver::getBit(uint8_t value, uint8_t bit)
{
    return (value & (1 << bit)) >> bit;
}
