// SPDX-License-Identifier: GPL-2.0-or-later
#include "PylontechCanReceiver.h"
#include "Configuration.h"
#include "MessageOutput.h"
#include "PinMapping.h"
#include <driver/twai.h>
#include <ctime>

//#define PYLONTECH_DEBUG_ENABLED

bool PylontechCanReceiver::init()
{
    MessageOutput.println(F("[Pylontech] Initialize interface..."));

    const PinMapping_t& pin = PinMapping.get();
    MessageOutput.printf("[Pylontech] Interface rx = %d, tx = %d\r\n",
            pin.battery_rx, pin.battery_tx);

    if (pin.battery_rx < 0 || pin.battery_tx < 0) {
        MessageOutput.println(F("[Pylontech] Invalid pin config"));
        return false;
    }

    auto tx = static_cast<gpio_num_t>(pin.battery_tx);
    auto rx = static_cast<gpio_num_t>(pin.battery_rx);
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(tx, rx, TWAI_MODE_NORMAL);

    // Initialize configuration structures using macro initializers
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Install TWAI driver
    esp_err_t twaiLastResult = twai_driver_install(&g_config, &t_config, &f_config);
    switch (twaiLastResult) {
        case ESP_OK:
            MessageOutput.println(F("[Pylontech] Twai driver installed"));
            break;
        case ESP_ERR_INVALID_ARG:
            MessageOutput.println(F("[Pylontech] Twai driver install - invalid arg"));
            return false;
            break;
        case ESP_ERR_NO_MEM:
            MessageOutput.println(F("[Pylontech] Twai driver install - no memory"));
            return false;
            break;
        case ESP_ERR_INVALID_STATE:
            MessageOutput.println(F("[Pylontech] Twai driver install - invalid state"));
            return false;
            break;
    }

    // Start TWAI driver
    twaiLastResult = twai_start();
    switch (twaiLastResult) {
        case ESP_OK:
            MessageOutput.println(F("[Pylontech] Twai driver started"));
            break;
        case ESP_ERR_INVALID_STATE:
            MessageOutput.println(F("[Pylontech] Twai driver start - invalid state"));
            return false;
            break;
    }

    return true;
}

void PylontechCanReceiver::deinit()
{
    // Stop TWAI driver
    esp_err_t twaiLastResult = twai_stop();
    switch (twaiLastResult) {
        case ESP_OK:
            MessageOutput.println(F("[Pylontech] Twai driver stopped"));
            break;
        case ESP_ERR_INVALID_STATE:
            MessageOutput.println(F("[Pylontech] Twai driver stop - invalid state"));
            break;
    }

    // Uninstall TWAI driver
    twaiLastResult = twai_driver_uninstall();
    switch (twaiLastResult) {
        case ESP_OK:
            MessageOutput.println(F("[Pylontech] Twai driver uninstalled"));
            break;
        case ESP_ERR_INVALID_STATE:
            MessageOutput.println(F("[Pylontech] Twai driver uninstall - invalid state"));
            break;
    }
}

void PylontechCanReceiver::loop()
{
    // Check for messages. twai_receive is blocking when there is no data so we return if there are no frames in the buffer
    twai_status_info_t status_info;
    esp_err_t twaiLastResult = twai_get_status_info(&status_info);
    if (twaiLastResult != ESP_OK) {
        switch (twaiLastResult) {
            case ESP_ERR_INVALID_ARG:
                MessageOutput.println(F("[Pylontech] Twai driver get status - invalid arg"));
                break;
            case ESP_ERR_INVALID_STATE:
                MessageOutput.println(F("[Pylontech] Twai driver get status - invalid state"));
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
        MessageOutput.println(F("[Pylontech] Failed to receive message"));
        return;
    }

    switch (rx_message.identifier) {
        case 0x351: {
            _stats->_chargeVoltage = this->scaleValue(this->readUnsignedInt16(rx_message.data), 0.1);
            _stats->_chargeCurrentLimitation = this->scaleValue(this->readSignedInt16(rx_message.data + 2), 0.1);
            _stats->_dischargeCurrentLimitation = this->scaleValue(this->readSignedInt16(rx_message.data + 4), 0.1);

#ifdef PYLONTECH_DEBUG_ENABLED
           MessageOutput.printf("[Pylontech] chargeVoltage: %f chargeCurrentLimitation: %f dischargeCurrentLimitation: %f\n",
                _stats->_chargeVoltage, _stats->_chargeCurrentLimitation, _stats->_dischargeCurrentLimitation);
#endif
            break;
        }

        case 0x355: {
            _stats->setSoC(static_cast<uint8_t>(this->readUnsignedInt16(rx_message.data)));
            _stats->_stateOfHealth = this->readUnsignedInt16(rx_message.data + 2);

#ifdef PYLONTECH_DEBUG_ENABLED
            MessageOutput.printf("[Pylontech] soc: %d soh: %d\n",
                _stats->getSoC(), _stats->_stateOfHealth);
#endif
            break;
        }

        case 0x356: {
            _stats->_voltage = this->scaleValue(this->readSignedInt16(rx_message.data), 0.01);
            _stats->_current = this->scaleValue(this->readSignedInt16(rx_message.data + 2), 0.1);
            _stats->_temperature = this->scaleValue(this->readSignedInt16(rx_message.data + 4), 0.1);

#ifdef PYLONTECH_DEBUG_ENABLED
            MessageOutput.printf("[Pylontech] voltage: %f current: %f temperature: %f\n",
                _stats->_voltage, _stats->_current, _stats->_temperature);
#endif
            break;
        }

        case 0x359: {
            uint16_t alarmBits = rx_message.data[0];
            _stats->_alarmOverCurrentDischarge = this->getBit(alarmBits, 7);
            _stats->_alarmUnderTemperature = this->getBit(alarmBits, 4);
            _stats->_alarmOverTemperature = this->getBit(alarmBits, 3);
            _stats->_alarmUnderVoltage = this->getBit(alarmBits, 2);
            _stats->_alarmOverVoltage= this->getBit(alarmBits, 1);

            alarmBits = rx_message.data[1];
            _stats->_alarmBmsInternal= this->getBit(alarmBits, 3);
            _stats->_alarmOverCurrentCharge = this->getBit(alarmBits, 0);

#ifdef PYLONTECH_DEBUG_ENABLED
            MessageOutput.printf("[Pylontech] Alarms: %d %d %d %d %d %d %d\n",
                _stats->_alarmOverCurrentDischarge,
                _stats->_alarmUnderTemperature,
                _stats->_alarmOverTemperature,
                _stats->_alarmUnderVoltage,
                _stats->_alarmOverVoltage,
                _stats->_alarmBmsInternal,
                _stats->_alarmOverCurrentCharge);
#endif

            uint16_t warningBits = rx_message.data[2];
            _stats->_warningHighCurrentDischarge = this->getBit(warningBits, 7);
            _stats->_warningLowTemperature = this->getBit(warningBits, 4);
            _stats->_warningHighTemperature = this->getBit(warningBits, 3);
            _stats->_warningLowVoltage = this->getBit(warningBits, 2);
            _stats->_warningHighVoltage = this->getBit(warningBits, 1);

            warningBits = rx_message.data[3];
            _stats->_warningBmsInternal= this->getBit(warningBits, 3);
            _stats->_warningHighCurrentCharge = this->getBit(warningBits, 0);

#ifdef PYLONTECH_DEBUG_ENABLED
            MessageOutput.printf("[Pylontech] Warnings: %d %d %d %d %d %d %d\n",
                _stats->_warningHighCurrentDischarge,
                _stats->_warningLowTemperature,
                _stats->_warningHighTemperature,
                _stats->_warningLowVoltage,
                _stats->_warningHighVoltage,
                _stats->_warningBmsInternal,
                _stats->_warningHighCurrentCharge);
#endif
            break;
        }

        case 0x35E: {
            String manufacturer(reinterpret_cast<char*>(rx_message.data),
                    rx_message.data_length_code);

            if (manufacturer.isEmpty()) { break; }

#ifdef PYLONTECH_DEBUG_ENABLED
            MessageOutput.printf("[Pylontech] Manufacturer: %s\n", manufacturer.c_str());
#endif

            _stats->setManufacturer(std::move(manufacturer));
            break;
        }

        case 0x35C: {
            uint16_t chargeStatusBits = rx_message.data[0];
            _stats->_chargeEnabled = this->getBit(chargeStatusBits, 7);
            _stats->_dischargeEnabled = this->getBit(chargeStatusBits, 6);
            _stats->_chargeImmediately = this->getBit(chargeStatusBits, 5);

#ifdef PYLONTECH_DEBUG_ENABLED
            MessageOutput.printf("[Pylontech] chargeStatusBits: %d %d %d\n",
                _stats->_chargeEnabled,
                _stats->_dischargeEnabled,
                _stats->_chargeImmediately);
#endif

            break;
        }

        default:
            return; // do not update last update timestamp
            break;
    }

    _stats->setLastUpdate(millis());
}

uint16_t PylontechCanReceiver::readUnsignedInt16(uint8_t *data)
{
    uint8_t bytes[2];
    bytes[0] = *data;
    bytes[1] = *(data + 1);
    return (bytes[1] << 8) + bytes[0];
}

int16_t PylontechCanReceiver::readSignedInt16(uint8_t *data)
{
    return this->readUnsignedInt16(data);
}

float PylontechCanReceiver::scaleValue(int16_t value, float factor)
{
    return value * factor;
}

bool PylontechCanReceiver::getBit(uint8_t value, uint8_t bit)
{
    return (value & (1 << bit)) >> bit;
}
