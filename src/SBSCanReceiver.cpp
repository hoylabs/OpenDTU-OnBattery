// SPDX-License-Identifier: GPL-2.0-or-later
#include "SBSCanReceiver.h"
#include "Configuration.h"
#include "MessageOutput.h"
#include "PinMapping.h"
#include <driver/twai.h>
#include <ctime>
//#define SBSCanReceiver_DUMMY

bool SBSCanReceiver::init(bool verboseLogging)
{
    _verboseLogging = verboseLogging;

    MessageOutput.println("[SBS Unipower XL] Initialize interface...");

    const PinMapping_t& pin = PinMapping.get();
    MessageOutput.printf("[SBS Unipower XL] Interface rx = %d, tx = %d\r\n",
            pin.battery_rx, pin.battery_tx);

    if (pin.battery_rx < 0 || pin.battery_tx < 0) {
        MessageOutput.println("[SBS Unipower XL] Invalid pin config");
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
            MessageOutput.println("[SBS Unipower XL] Twai driver installed");
            break;
        case ESP_ERR_INVALID_ARG:
            MessageOutput.println("[SBS Unipower XL] Twai driver install - invalid arg");
            return false;
            break;
        case ESP_ERR_NO_MEM:
            MessageOutput.println("[SBS Unipower XL] Twai driver install - no memory");
            return false;
            break;
        case ESP_ERR_INVALID_STATE:
            MessageOutput.println("[SBS Unipower XL] Twai driver install - invalid state");
            return false;
            break;
    }

    // Start TWAI driver
    twaiLastResult = twai_start();
    switch (twaiLastResult) {
        case ESP_OK:
            MessageOutput.println("[SBS Unipower XL] Twai driver started");
            break;
        case ESP_ERR_INVALID_STATE:
            MessageOutput.println("[SBS Unipower XL] Twai driver start - invalid state");
            return false;
            break;
    }

    return true;
}

void SBSCanReceiver::deinit()
{
    // Stop TWAI driver
    esp_err_t twaiLastResult = twai_stop();
    switch (twaiLastResult) {
        case ESP_OK:
            MessageOutput.println("[SBS Unipower XL] Twai driver stopped");
            break;
        case ESP_ERR_INVALID_STATE:
            MessageOutput.println("[SBS Unipower XL] Twai driver stop - invalid state");
            break;
    }

    // Uninstall TWAI driver
    twaiLastResult = twai_driver_uninstall();
    switch (twaiLastResult) {
        case ESP_OK:
            MessageOutput.println("[SBS Unipower XL] Twai driver uninstalled");
            break;
        case ESP_ERR_INVALID_STATE:
            MessageOutput.println("[SBS Unipower XL] Twai driver uninstall - invalid state");
            break;
    }
}

void SBSCanReceiver::loop()
{
#ifdef SBSCanReceiver_DUMMY
    return dummyData();
#endif

    // Check for messages. twai_receive is blocking when there is no data so we return if there are no frames in the buffer
    twai_status_info_t status_info;
    esp_err_t twaiLastResult = twai_get_status_info(&status_info);
    if (twaiLastResult != ESP_OK) {
        switch (twaiLastResult) {
            case ESP_ERR_INVALID_ARG:
                MessageOutput.println("[SBS Unipower XL] Twai driver get status - invalid arg");
                break;
            case ESP_ERR_INVALID_STATE:
                MessageOutput.println("[SBS Unipower XL] Twai driver get status - invalid state");
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
        MessageOutput.println("[SBS Unipower XL] Failed to receive message");
        return;
    }
    switch (rx_message.identifier) {
        case 0x610: {
            _stats->setVoltage(this->readUnsignedInt16(rx_message.data)* 0.001, millis());
            _stats->_current =(this->readSignedInt24(rx_message.data + 3)) * 0.001;
            _stats->setSoC(static_cast<uint8_t>(this->readUnsignedInt16(rx_message.data + 6)), 1, millis());
            if (_verboseLogging) {
                MessageOutput.printf("[SBS Unipower XL 1552] SoC: %i Voltage: %f Current: %f\n", this->readUnsignedInt16(rx_message.data + 6), this->readUnsignedInt16(rx_message.data + 0)*0.001, _stats->_current);
            }
            break;
        }
         case 0x630: {
            _stats->_dischargeEnabled = 0;
            _stats->_chargeEnabled = 0;
            _stats->_chargeVoltage =58.4;
            int clusterstate = rx_message.data[0];
            switch (clusterstate) {
                case 0:_stats->setManufacturer(std::move("SBS UniPower XL Inactive"));
                break;
                case 1: {
                    _stats->setManufacturer(std::move("SBS UniPower XL Discharge"));
                    _stats->_chargeEnabled = 1;
                    _stats->_dischargeEnabled = 1;
                    break;
                }
                case 2: {
                    _stats->setManufacturer(std::move("SBS UniPower XL Charge"));
                    _stats->_chargeEnabled = 1;
                    break;
                }
                case 4:_stats->setManufacturer(std::move("SBS UniPower XL Fault"));
                break;
                case 8: _stats->setManufacturer(std::move("SBS UniPower XL Deepsleep"));
                break;
                default: _stats->setManufacturer(std::move("SBS UniPower XL XXX"));
                break;
            }
            if (_verboseLogging) {
                MessageOutput.printf("[SBS Unipower] 1584 chargeStatusBits: %d %d\n", _stats->_chargeEnabled, _stats->_dischargeEnabled);
            }
            break;
        }
        case 0x640: {
            _stats->_chargeCurrentLimitation = (this->readSignedInt24(rx_message.data + 3) * 0.001);
            _stats->_dischargeCurrentLimitation = (this->readSignedInt24(rx_message.data)) * 0.001;
            if (_verboseLogging) {
                MessageOutput.printf("[SBS Unipower] 1600  Packet %02x %02x %02x %02x %02x %02x\n",*(rx_message.data),*(rx_message.data +1),*(rx_message.data +2),*(rx_message.data +3),*(rx_message.data +4),*(rx_message.data +5));
            }
            break;
        }
        case 0x650: {
            byte temp = rx_message.data[0];
            _stats->_temperature = (float(temp)-32) /1.8;
            if (_verboseLogging) {
                MessageOutput.printf("[SBS Unipower] 1616  Packet %02x %02x \n",*(rx_message.data),*(rx_message.data +1));
            }
            break;
        }
        case 0x660: {
            uint16_t alarmBits = rx_message.data[0];
            _stats->_alarmUnderTemperature = this->getBit(alarmBits, 1);
            _stats->_alarmOverTemperature = this->getBit(alarmBits, 0);
            _stats->_alarmUnderVoltage = this->getBit(alarmBits, 3);
            _stats->_alarmOverVoltage= this->getBit(alarmBits, 2);

            alarmBits = rx_message.data[1];
            _stats->_alarmBmsInternal= this->getBit(alarmBits, 2);
            if (_verboseLogging) {
                MessageOutput.printf("[SBS 1632] Alarms: %d %d %d %d \n ", _stats->_alarmUnderTemperature, _stats->_alarmOverTemperature, _stats->_alarmUnderVoltage,  _stats->_alarmOverVoltage);
            }
            break;
        }
        case 0x670: {
            uint16_t warningBits = rx_message.data[1];
            _stats->_warningHighCurrentDischarge = this->getBit(warningBits, 1);
            _stats->_warningHighCurrentCharge = this->getBit(warningBits, 0);

             if (_verboseLogging) {
                MessageOutput.printf("[SBS Unipower] 1648 Warnings: %d %d \n", _stats->_warningHighCurrentDischarge, _stats->_warningHighCurrentCharge);
            }
            break;
        }
        default:
            return; // do not update last update timestamp
            break;
    }

    _stats->setLastUpdate(millis());
}

uint8_t SBSCanReceiver::readUnsignedInt8(uint8_t *data)
{
    uint8_t bytes[1];
    bytes[0] = *data;
    return bytes[0];
}

uint16_t SBSCanReceiver::readUnsignedInt16(uint8_t *data)
{
    return (data[2] <<16) | (data[1] <<8) | data[0];
}

int SBSCanReceiver::readSignedInt24(uint8_t *data)
{
return (int32_t)((int32_t)data[0] | (((int32_t)data[1]) << 8) | (((int32_t)data[2]) << 16)) ;
}

bool SBSCanReceiver::getBit(uint8_t value, uint8_t bit)
{
    return (value & (1 << bit)) >> bit;
}

#ifdef SBSCanReceiver_DUMMY
void SBSCanReceiver::dummyData()
{
    static uint32_t lastUpdate = millis();
    static uint8_t issues = 0;

    if (millis() < (lastUpdate + 5 * 1000)) { return; }

    lastUpdate = millis();
    _stats->setLastUpdate(lastUpdate);

    auto dummyFloat = [](int offset) -> float {
        return offset + (static_cast<float>((lastUpdate + offset) % 10) / 10);
    };

    _stats->setManufacturer("SBS Unipower XL");
    _stats->setSoC(42, 0/*precision*/, millis());
    _stats->_chargeVoltage = dummyFloat(50);
    _stats->_chargeCurrentLimitation = dummyFloat(33);
    _stats->_dischargeCurrentLimitation = dummyFloat(12);
    _stats->_stateOfHealth = 99;
    _stats->setVoltage(48.67, millis());
    _stats->_current = dummyFloat(-1);
    _stats->_temperature = dummyFloat(20);

    _stats->_chargeEnabled = true;
    _stats->_dischargeEnabled = true;

    _stats->_warningHighCurrentDischarge = false;
    _stats->_warningHighCurrentCharge = false;

    _stats->_alarmOverCurrentDischarge = false;
    _stats->_alarmOverCurrentCharge = false;
    _stats->_alarmUnderVoltage = false;
    _stats->_alarmOverVoltage = false;


    if (issues == 1 || issues == 3) {
        _stats->_warningHighCurrentDischarge = true;
        _stats->_warningHighCurrentCharge = true;
    }

    if (issues == 2 || issues == 3) {
        _stats->_alarmOverCurrentDischarge = true;
        _stats->_alarmOverCurrentCharge = true;
        _stats->_alarmUnderVoltage = true;
        _stats->_alarmOverVoltage = true;
    }

    if (issues == 4) {
        _stats->_warningHighCurrentCharge = true;
        _stats->_alarmUnderVoltage = true;
        _stats->_dischargeEnabled = false;
    }

    issues = (issues + 1) % 5;
}
#endif
