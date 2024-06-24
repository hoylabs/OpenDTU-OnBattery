// SPDX-License-Identifier: GPL-2.0-or-later
#include "SBSCanReceiver.h"
#include "MessageOutput.h"
#include "PinMapping.h"
#include <driver/twai.h>
#include <ctime>

bool SBSCanReceiver::init(bool verboseLogging)
{
    return BatteryCanReceiver::init(verboseLogging, "SBS Unipower XL");
}


void SBSCanReceiver::onMessage(twai_message_t rx_message)
{
    switch (rx_message.identifier) {
        case 0x610: {
            _stats->setVoltage(this->readUnsignedInt16(rx_message.data)* 0.001, millis());
            _stats->_current =(this->readSignedInt24(rx_message.data + 3)) * 0.001;
            _stats->setSoC(static_cast<float>(this->readUnsignedInt16(rx_message.data + 6)), 1, millis());
            if (_verboseLogging) {
                MessageOutput.printf("[SBS Unipower XL] 1552 SoC: %i Voltage: %f Current: %f\n", _stats->getSoC(), _stats->getVoltage(), _stats->_current);
            }
            break;
        }
         case 0x630: {
            String state = "";
            _stats->_dischargeEnabled = 0;
            _stats->_chargeEnabled = 0;
            _stats->_chargeVoltage =58.4;
            int clusterstate = rx_message.data[0];
            switch (clusterstate) {
                case 0:state="Inactive";
                break;
                case 1: {
                    state="Discharge";
                    _stats->_chargeEnabled = 1;
                    _stats->_dischargeEnabled = 1;
                    break;
                }
                case 2: {
                    state="Charge";
                    _stats->_chargeEnabled = 1;
                    break;
                }
                case 4:state="Fault";
                break;
                case 8: state="Deepsleep";
                break;
                default:
                break;
            }
            _stats->setManufacturer(std::move("SBS UniPower XL " + state));
            if (_verboseLogging) {
                MessageOutput.printf("[SBS] 1584 chargeStatusBits: %d %d\n", _stats->_chargeEnabled, _stats->_dischargeEnabled);
            }
            break;
        }
        case 0x640: {
            _stats->_chargeCurrentLimitation = (this->readSignedInt24(rx_message.data + 3) * 0.001);
            _stats->_dischargeCurrentLimitation = (this->readSignedInt24(rx_message.data)) * 0.001;
            if (_verboseLogging) {
                MessageOutput.printf("[SBS] 1600  %f, %f \n", _stats->_chargeCurrentLimitation, _stats->_dischargeCurrentLimitation);
            }
            break;
        }
        case 0x650: {
            byte temp = rx_message.data[0];
            _stats->_temperature = (float(temp)-32) /1.8;
            if (_verboseLogging) {
                MessageOutput.printf("[SBS] 1616  Temp %f \n",_stats->_temperature);
            }
            break;
        }
        case 0x660: {
            uint16_t alarmBits = rx_message.data[0];
            _stats->_alarmUnderTemperature = this->getBit(alarmBits, 1);
            _stats->_alarmOverTemperature = this->getBit(alarmBits, 0);
            _stats->_alarmUnderVoltage = this->getBit(alarmBits, 3);
            _stats->_alarmOverVoltage= this->getBit(alarmBits, 2);

            _stats->_alarmBmsInternal= this->getBit(rx_message.data[1], 2);
            if (_verboseLogging) {
                MessageOutput.printf("[SBS] 1632 Alarms: %d %d %d %d \n ", _stats->_alarmUnderTemperature, _stats->_alarmOverTemperature, _stats->_alarmUnderVoltage,  _stats->_alarmOverVoltage);
            }
            break;
        }
        case 0x670: {
            uint16_t warningBits = rx_message.data[1];
            _stats->_warningHighCurrentDischarge = this->getBit(warningBits, 1);
            _stats->_warningHighCurrentCharge = this->getBit(warningBits, 0);

             if (_verboseLogging) {
                MessageOutput.printf("[SBS] 1648 Warnings: %d %d \n", _stats->_warningHighCurrentDischarge, _stats->_warningHighCurrentCharge);
            }
            break;
        }
        default:
            return;
            break;
    }

    _stats->setLastUpdate(millis());
}

int SBSCanReceiver::readSignedInt24(uint8_t *data)
{
return (int32_t)((int32_t)data[0] | (((int32_t)data[1]) << 8) | (((int32_t)data[2]) << 16)) ;
}
