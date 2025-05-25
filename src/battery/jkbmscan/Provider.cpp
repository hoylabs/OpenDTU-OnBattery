// SPDX-License-Identifier: GPL-2.0-or-later
#include <battery/jkbmscan/Provider.h>
#include <MessageOutput.h>
#include <PinMapping.h>
#include <driver/twai.h>
#include <ctime>

namespace Batteries::JkBmsCan {

Provider::Provider()
    : _stats(std::make_shared<Stats>())
    , _hassIntegration(std::make_shared<HassIntegration>(_stats)) { }

bool Provider::init(bool verboseLogging)
{
    return ::Batteries::CanReceiver::init(verboseLogging, "JkBmsCan");
}

void Provider::onMessage(twai_message_t rx_message)
{
    switch (rx_message.identifier) {
        case 0x02F4: {
            _stats->setVoltage(this->scaleValue(this->readSignedInt16(rx_message.data), 0.1), millis());
            _stats->setCurrent((this->scaleValue(this->readSignedInt16(rx_message.data + 2), 0.1)-400.0), 1/*precision*/, millis());
            _stats->setSoC(static_cast<uint8_t>(this->readUnsignedInt8(rx_message.data + 4)), 0/*precision*/, millis());
            String manufacturer = "JKBMS ID: 0";
            if (manufacturer.isEmpty()) { break; }

            if (_verboseLogging) {
                MessageOutput.printf("[JkBmsCan] Manufacturer: %s\r\n", manufacturer.c_str());
            }

            _stats->setManufacturer(manufacturer);
            break;
        }
        case 0x04F4: {
            _stats->_MaxCellVoltage=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data)));
            _stats->_MaxCellVoltageNumber=(static_cast<uint8_t>(this->readUnsignedInt8(rx_message.data+2)));
            _stats->_MinCellVoltage=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data+3)));
            _stats->_MinCellVoltageNumber=(static_cast<uint8_t>(this->readUnsignedInt8(rx_message.data+5)));
            break;
        }

        case 0x05F4: {
            _stats->_temperature = (static_cast<uint8_t>(this->readUnsignedInt8(rx_message.data + 4))) - 50.0;
            if (_verboseLogging) {
                MessageOutput.printf("[JkBmsCan] voltage: %f current: %f temperature: %f\r\n",
                        _stats->getVoltage(), _stats->getChargeCurrent(), _stats->_temperature);
            }
            break;
        }
 

        case 0x18E028F4: {
            _stats->_cellVoltage[0]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data)));
            _stats->_cellVoltage[1]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data+2)));
            _stats->_cellVoltage[2]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data+4)));
            _stats->_cellVoltage[3]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data+6)));
            break;
        }

        case 0x18E128F4: {
            _stats->_cellVoltage[4]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data)));
            _stats->_cellVoltage[5]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data+2)));
            _stats->_cellVoltage[6]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data+4)));
            _stats->_cellVoltage[7]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data+6)));
            break;
        }

        case 0x18E228F4: {
            _stats->_cellVoltage[8]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data)));
            _stats->_cellVoltage[9]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data+2)));
            _stats->_cellVoltage[10]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data+4)));
            _stats->_cellVoltage[11]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data+6)));
            break;
        }
        case 0x18E328F4: {
            _stats->_cellVoltage[12]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data)));
            _stats->_cellVoltage[13]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data+2)));
            _stats->_cellVoltage[14]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data+4)));
            _stats->_cellVoltage[15]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data+6)));
            break;
        }
        case 0x18E428F4: {
            _stats->_cellVoltage[16]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data)));
            _stats->_cellVoltage[17]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data+2)));
            _stats->_cellVoltage[18]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data+4)));
            _stats->_cellVoltage[19]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data+6)));
            break;
        }
        case 0x18E528F4: {
            _stats->_cellVoltage[20]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data)));
            _stats->_cellVoltage[21]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data+2)));
            _stats->_cellVoltage[22]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data+4)));
            _stats->_cellVoltage[23]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data+6)));
            break;
        }
        case 0x18E628F4: {
            _stats->_cellVoltage[24]=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data)));
            break;
        }

        case 0x18F128F4: {
            _stats->_capacityRemaining = this->scaleValue(this->readUnsignedInt16(rx_message.data), 0.1);
            _stats->_fullChargeCapacity = this->scaleValue(this->readUnsignedInt16(rx_message.data+2), 0.1);
            _stats->_cycleCapacity =  this->scaleValue(this->readUnsignedInt16(rx_message.data+4), 0.1);
            _stats->_cycleCount=(static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data+6)));
            break;
        }

        case 0x18F328F4: {
            uint16_t alarmBits = rx_message.data[0];
            _stats->_alarmOverCurrentDischarge = this->getBit(alarmBits, 7);
            _stats->_alarmUnderTemperature = this->getBit(alarmBits, 4);
            _stats->_alarmOverTemperature = this->getBit(alarmBits, 3);
            _stats->_alarmUnderVoltage = this->getBit(alarmBits, 2);
            _stats->_alarmOverVoltage= this->getBit(alarmBits, 1);

            alarmBits = rx_message.data[1];
            _stats->_alarmBmsInternal= this->getBit(alarmBits, 3);
            _stats->_alarmOverCurrentCharge = this->getBit(alarmBits, 0);

            if (_verboseLogging) {
                MessageOutput.printf("[JkBmsCan] Alarms: %d %d %d %d %d %d %d\r\n",
                        _stats->_alarmOverCurrentDischarge,
                        _stats->_alarmUnderTemperature,
                        _stats->_alarmOverTemperature,
                        _stats->_alarmUnderVoltage,
                        _stats->_alarmOverVoltage,
                        _stats->_alarmBmsInternal,
                        _stats->_alarmOverCurrentCharge);
            }

            uint16_t warningBits = rx_message.data[2];
            _stats->_warningHighCurrentDischarge = this->getBit(warningBits, 7);
            _stats->_warningLowTemperature = this->getBit(warningBits, 4);
            _stats->_warningHighTemperature = this->getBit(warningBits, 3);
            _stats->_warningLowVoltage = this->getBit(warningBits, 2);
            _stats->_warningHighVoltage = this->getBit(warningBits, 1);

            warningBits = rx_message.data[3];
            _stats->_warningBmsInternal= this->getBit(warningBits, 3);
            _stats->_warningHighCurrentCharge = this->getBit(warningBits, 0);

            if (_verboseLogging) {
                MessageOutput.printf("[JkBmsCan] Warnings: %d %d %d %d %d %d %d\r\n",
                        _stats->_warningHighCurrentDischarge,
                        _stats->_warningLowTemperature,
                        _stats->_warningHighTemperature,
                        _stats->_warningLowVoltage,
                        _stats->_warningHighVoltage,
                        _stats->_warningBmsInternal,
                        _stats->_warningHighCurrentCharge);
            }
            break;
        }

        case 0x18F428F4: {
            _stats->_bmsRunTime = this->readUnsignedInt32(rx_message.data);
            _stats->_heaterCurrent = this->readUnsignedInt16(rx_message.data + 4);
            _stats->_stateOfHealth = this->readUnsignedInt8(rx_message.data + 6);
            break;
        }

        case 0x18F528F4: {
            uint16_t chargeStatusBits = rx_message.data[0];
            _stats->_chargeEnabled = this->getBit(chargeStatusBits, 0);
            _stats->_dischargeEnabled = this->getBit(chargeStatusBits, 1);
            _stats->_balanceEnabled = this->getBit(chargeStatusBits, 2);
            _stats->_heaterEnabled = this->getBit(chargeStatusBits, 3);
            _stats->_chargerPluged = this->getBit(chargeStatusBits, 4);
            _stats->_accEnabled = this->getBit(chargeStatusBits, 5);

            if (_verboseLogging) {
                MessageOutput.printf("[JkBmsCan] chargeStatusBits: %d %d %d\r\n",
                    _stats->_chargeEnabled,
                    _stats->_dischargeEnabled,
                    _stats->_balanceEnabled);
            }

            break;
        }

        case 0x1806E5F4: {
            _stats->_chargeVoltage = this->scaleValue(this->readBigEndianUnsignedInt16(rx_message.data), 0.1);
            _stats->_chargeCurrentLimitation = this->scaleValue(this->readBigEndianUnsignedInt16(rx_message.data + 2), 0.1);
            _stats->_chargeRequest = this->readUnsignedInt8(rx_message.data + 4);
            _stats->_chargeAndHeat = this->readUnsignedInt8(rx_message.data + 5);



            if (_verboseLogging) {
                MessageOutput.printf("[JkBmsCan] chargeVoltage: %f chargeCurrentLimitation: %f chargeRequest: %d chargeAndHeat: %d\r\n",
                        _stats->_chargeVoltage, _stats->_chargeCurrentLimitation, _stats->_chargeRequest,
                        _stats->_chargeAndHeat);
            }
            break;
        }

        default:
            return; // do not update last update timestamp
            break;
    }

    _stats->setLastUpdate(millis());
}



} // namespace Batteries::JkBmsCan
