// SPDX-License-Identifier: GPL-2.0-or-later
#include <battery/jkbmscan/Provider.h>
#include <MessageOutput.h>
#include <PinMapping.h>
#include <driver/twai.h>
#include <ctime>
#include <Configuration.h>
#include <LogHelper.h>

static const char *TAG = "battery";
static const char *SUBTAG = "JkBmsCan";

namespace Batteries::JkBmsCan
{

    Provider::Provider()
        : _stats(std::make_shared<Stats>()), _hassIntegration(std::make_shared<HassIntegration>(_stats)) {}

    bool Provider::init()
    {
        return ::Batteries::CanReceiver::init("JkBmsCan");
    }

    // The JK BMS allows configuring a device ID.
    // This ID is encoded in the lower 8 bits of the CAN identifier (starting at 0xF4).
    // This function filters incoming frames and only processes those matching the configured BMS ID.
    bool Provider::isSelectedBms(uint32_t can_id, uint8_t configuredId)
    {
        uint8_t src = can_id & 0xFF;

        if (src < 0xF4)
            return false;

        return (src - 0xF4) == configuredId;
    }

    void Provider::updateCellCountIfNeeded()
    {
        auto const &config = Configuration.get();
        uint8_t cfg = config.Battery.JkBmsCan.NumberOfCells;

        if (cfg != _lastConfiguredCells)
        {
            _lastConfiguredCells = cfg;

            _cellCount = std::min<uint8_t>(cfg, Stats::MAX_CELLS);

            if (cfg > Stats::MAX_CELLS)
            {
                DTU_LOGW("[JkBmsCan] Configured cells (%d) exceed max (%d), clamped",
                         cfg, Stats::MAX_CELLS);
            }
        }
    }

    void Provider::onMessage(twai_message_t rx_message)
    {
        auto const &config = Configuration.get();

        if (!isSelectedBms(rx_message.identifier, config.Battery.JkBmsCan.configuredId))
        {
            return;
        }

        // Check for configuration changes and update cell count within limits.
        updateCellCountIfNeeded();

        switch (rx_message.identifier & 0xFFFFFF00)
        {
            uint32_t now = millis();
        case 0x0200:
        {
            // CAN protocol v1 provides pack voltage directly in this frame.
            // For newer versions, pack voltage is derived from cell voltages for better accuracy.
            if (config.Battery.JkBmsCan.CanProtocolVersion == 1)
            {
                _stats->setVoltage(this->scaleValue(this->readSignedInt16(rx_message.data), 0.1), now);
            }
            else
            {
                _stats->_packVoltage = 0;
                for (int i = 0; i < config.Battery.JkBmsCan.NumberOfCells; i++)
                {
                    _stats->_packVoltage += _stats->_cellVoltage[i];
                }
                _stats->setVoltage(this->scaleValue((_stats->_packVoltage), 0.001), millis());
            }
            _stats->setCurrent((this->scaleValue(this->readSignedInt16(rx_message.data + 2), 0.1) - 400.0), 1 /*precision*/, now);
            _stats->setSoC(static_cast<uint8_t>(this->readUnsignedInt8(rx_message.data + 4)), 0 /*precision*/, now);

            String manufacturer = "JKBMS ID: " + String((rx_message.identifier & 0x000000FF) - 0xF4);

            DTU_LOGD("[JkBmsCan] Manufacturer: %s\r\n", manufacturer.c_str());

            _stats->setManufacturer(manufacturer);
            break;
        }
        case 0x0400:
        {
            _stats->_MaxCellVoltage = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data)));
            _stats->_MaxCellVoltageNumber = (static_cast<uint8_t>(this->readUnsignedInt8(rx_message.data + 2)));
            _stats->_MinCellVoltage = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data + 3)));
            _stats->_MinCellVoltageNumber = (static_cast<uint8_t>(this->readUnsignedInt8(rx_message.data + 5)));
            break;
        }

        case 0x0500:
        {
            _stats->_temperature = (static_cast<uint8_t>(this->readUnsignedInt8(rx_message.data + 4))) - 50.0;
            DTU_LOGD("[JkBmsCan] voltage: %f current: %f temperature: %f",
                     _stats->getVoltage(), _stats->getChargeCurrent(), _stats->_temperature);

            break;
        }

        case 0x0700:
        {

            DTU_LOGV("[JkBmsCan] V1 ID:0x%08X raw: %02X %02X %02X %02X %02X %02X %02X %02X",
                     rx_message.identifier,
                     rx_message.data[0],
                     rx_message.data[1],
                     rx_message.data[2],
                     rx_message.data[3],
                     rx_message.data[4],
                     rx_message.data[5],
                     rx_message.data[6],
                     rx_message.data[7]);
            // Just provide data for stats. The data is combined with the V2 frame data in _stats.evaluateErrors

            _stats.updateFromV1(rx_message.data, millis());
            break;
        }

        case 0x18E02800:
        {
            // When this frame is received, the BMS is reporting single cell voltages and the pack voltage can be calculated from the single cell voltages.
            // It
            _stats->_cellVoltage[0] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data)));
            _stats->_cellVoltage[1] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data + 2)));
            _stats->_cellVoltage[2] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data + 4)));
            _stats->_cellVoltage[3] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data + 6)));
            break;
        }

        case 0x18E12800:
        {
            _stats->_cellVoltage[4] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data)));
            _stats->_cellVoltage[5] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data + 2)));
            _stats->_cellVoltage[6] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data + 4)));
            _stats->_cellVoltage[7] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data + 6)));
            break;
        }

        case 0x18E22800:
        {
            _stats->_cellVoltage[8] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data)));
            _stats->_cellVoltage[9] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data + 2)));
            _stats->_cellVoltage[10] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data + 4)));
            _stats->_cellVoltage[11] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data + 6)));
            break;
        }
        case 0x18E32800:
        {
            _stats->_cellVoltage[12] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data)));
            _stats->_cellVoltage[13] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data + 2)));
            _stats->_cellVoltage[14] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data + 4)));
            _stats->_cellVoltage[15] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data + 6)));
            break;
        }
        case 0x18E42800:
        {
            _stats->_cellVoltage[16] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data)));
            _stats->_cellVoltage[17] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data + 2)));
            _stats->_cellVoltage[18] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data + 4)));
            _stats->_cellVoltage[19] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data + 6)));
            break;
        }
        case 0x18E52800:
        {
            _stats->_cellVoltage[20] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data)));
            _stats->_cellVoltage[21] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data + 2)));
            _stats->_cellVoltage[22] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data + 4)));
            _stats->_cellVoltage[23] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data + 6)));
            break;
        }
        case 0x18E62800:
        {
            _stats->_cellVoltage[24] = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data)));
            break;
        }

        case 0x18F12800:
        {
            _stats->_capacityRemaining = this->scaleValue(this->readUnsignedInt16(rx_message.data), 0.1);
            _stats->_fullChargeCapacity = this->scaleValue(this->readUnsignedInt16(rx_message.data + 2), 0.1);
            _stats->_cycleCapacity = this->scaleValue(this->readUnsignedInt16(rx_message.data + 4), 0.1);
            _stats->_cycleCount = (static_cast<uint16_t>(this->readUnsignedInt16(rx_message.data + 6)));
            break;
        }

        case 0x18F32800:
        {
            // Raw logging for debugging
            DTU_LOGV("[JkBmsCan] ID:0x18F32800 raw: %02X %02X %02X %02X",
                     rx_message.data[0],
                     rx_message.data[1],
                     rx_message.data[2],
                     rx_message.data[3]);

            // Just provide data for stats. The data is combined with the V1 frame data in _stats.evaluateErrors
            _stats.updateFromV2(rx_message.data, now);
        }

        break;

        case 0x18F42800:
        {
            _stats->_bmsRunTime = this->readUnsignedInt32(rx_message.data);
            _stats->_heaterCurrent = this->readUnsignedInt16(rx_message.data + 4);
            _stats->_stateOfHealth = this->readUnsignedInt8(rx_message.data + 6);
            break;
        }

        case 0x18F52800:
        {
            uint16_t chargeStatusBits = rx_message.data[0];
            _stats->_chargeEnabled = this->getBit(chargeStatusBits, 0);
            _stats->_dischargeEnabled = this->getBit(chargeStatusBits, 1);
            _stats->_balanceEnabled = this->getBit(chargeStatusBits, 2);
            _stats->_heaterEnabled = this->getBit(chargeStatusBits, 3);
            _stats->_chargerPluged = this->getBit(chargeStatusBits, 4);
            _stats->_accEnabled = this->getBit(chargeStatusBits, 5);

            DTU_LOGD("[JkBmsCan] chargeStatusBits: %d %d %d",
                     _stats->_chargeEnabled,
                     _stats->_dischargeEnabled,
                     _stats->_balanceEnabled);

            break;
        }

        case 0x1806E500:
        {
            _stats->_chargeVoltage = this->scaleValue(this->readBigEndianUnsignedInt16(rx_message.data), 0.1);
            _stats->_chargeCurrentLimitation = this->scaleValue(this->readBigEndianUnsignedInt16(rx_message.data + 2), 0.1);
            _stats->_chargeRequest = this->readUnsignedInt8(rx_message.data + 4);
            _stats->_chargeAndHeat = this->readUnsignedInt8(rx_message.data + 5);

            DTU_LOGD("[JkBmsCan] chargeVoltage: %f chargeCurrentLimitation: %f chargeRequest: %d chargeAndHeat: %d\r\n",
                     _stats->_chargeVoltage, _stats->_chargeCurrentLimitation, _stats->_chargeRequest,
                     _stats->_chargeAndHeat);
            break;
        }

        default:
            return; // do not update last update timestamp
            break;
        }

        _stats->setLastUpdate(now);
    }
    om
} // namespace Batteries::JkBmsCan
