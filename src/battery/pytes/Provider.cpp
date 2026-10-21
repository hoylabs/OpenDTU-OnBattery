// SPDX-License-Identifier: GPL-2.0-or-later
#include <battery/pytes/Provider.h>
#include <algorithm>
#include <PinMapping.h>
#include <driver/twai.h>
#include <ctime>
#include <LogHelper.h>

#undef TAG
static const char* TAG = "battery";
static const char* SUBTAG = "Pytes";

namespace Batteries::Pytes {

Provider::Provider()
    : _stats(std::make_shared<Stats>())
    , _hassIntegration(std::make_shared<HassIntegration>(_stats)) { }

// present a cell (or temperature sensor) and its module as "<module>-<cell>",
// 1-based, like the Pytes RS485 provider does. both inputs are 0-based.
static void setCellLabel(String& label, uint8_t cell, uint8_t module) {
    char name[8];
    snprintf(name, sizeof(name), "%d-%d", module + 1, cell + 1);
    label = name;  // updates existing string in-place
}

// Pytes protocol (0x401/0x402): the label is a 16-bit value, low byte =
// 0-based cell (or temperature sensor), high byte = 0-based module.
static void pytesSetCellLabel(String& label, uint16_t packed) {
    setCellLabel(label, packed & 0xff, packed >> 8);
}

// Victron protocol (0x374-0x377): four ASCII digits, 0-based cell (or
// temperature sensor) number followed by the 0-based module number, e.g.
// "0500". presented like the Pytes protocol labels above.
static void victronSetCellLabel(String& label, String const& value) {
    auto digit = [&value](unsigned i) { return isdigit(static_cast<unsigned char>(value[i])); };
    if (value.length() >= 4 && digit(0) && digit(1) && digit(2) && digit(3)) {
        uint8_t cell = (value[0] - '0') * 10 + (value[1] - '0');
        uint8_t module = (value[2] - '0') * 10 + (value[3] - '0');
        setCellLabel(label, cell, module);
        return;
    }

    label = value;
}

// raw frame content for debug logs, to analyze fields that are not (fully)
// decoded yet
static String hexBytes(uint8_t const* data, uint8_t len) {
    String out;
    char buf[4];
    for (uint8_t i = 0; i < len; ++i) {
        snprintf(buf, sizeof(buf), i ? " %02X" : "%02X", data[i]);
        out += buf;
    }
    return out;
}

bool Provider::init()
{
    return ::Batteries::CanReceiver::init("Pytes");
}

void Provider::onMessage(twai_message_t rx_message)
{
    // module firmware version replies (extended id). must NOT blanket-drop
    // other extended frames: the battery sends its regular frames as extended
    // ids too, they have to reach the switch below.
    if (rx_message.extd) {
        if (rx_message.identifier >= 0x19A61 && rx_message.identifier <= 0x19A65) {
            onModuleVersionFrame(rx_message.identifier, rx_message.data, rx_message.data_length_code);
            return;
        }
    }

    // per-module cell data replies (0x4F0 + module * 0x10 + n), not used by
    // the regular protocol. matched on the raw id: only the default group (0) is
    // supported, a non-default group's reply (id + group * 0x1000) stays unmatched.
    if (rx_message.identifier >= 0x500 && rx_message.identifier <= 0x5FF) {
        onModuleCellFrame(rx_message.identifier, rx_message.data, rx_message.data_length_code);
        return;
    }

    // only the default CAN group (0) is supported: its native frames carry no
    // group offset (id = base), so the base ids below match directly. frames of
    // a non-default group (id = base + group * 0x1000) stay unmatched.
    uint32_t id = rx_message.identifier;

    // per-module requests are only answered in the Pytes native protocol
    // (0x400-0x41E); the Victron protocol rejects them. remember when such a
    // frame was last seen so onLoop() only requests then.
    if (id >= 0x400 && id <= 0x41E) {
        _lastPytesFrame = millis();
    }

    switch (id) {
        case 0x351:
        case 0x400: {
            _stats->_chargeVoltageLimit = this->scaleValue(this->readUnsignedInt16(rx_message.data), 0.1);
            _stats->setChargeCurrentLimit(this->scaleValue(this->readUnsignedInt16(rx_message.data + 2), 0.1), millis());
            _stats->setDischargeCurrentLimit(this->scaleValue(this->readUnsignedInt16(rx_message.data + 4), 0.1), millis());
            _stats->_dischargeVoltageLimit = this->scaleValue(this->readSignedInt16(rx_message.data + 6), 0.1);

            DTU_LOGD("chargeVoltageLimit: %f chargeCurrentLimit: "
                    "%f dischargeCurrentLimit: %f dischargeVoltageLimit: %f",
                    _stats->_chargeVoltageLimit, _stats->getChargeCurrentLimit(),
                    _stats->getDischargeCurrentLimit(), _stats->_dischargeVoltageLimit);
            break;
        }

        case 0x355: {  // Victron protocol: SOC/SOH
            _stats->setSoC(static_cast<uint8_t>(this->readUnsignedInt16(rx_message.data)), 0/*precision*/, millis());
            _stats->_stateOfHealth = this->readUnsignedInt16(rx_message.data + 2);

            DTU_LOGD("soc: %f soh: %d", _stats->getSoC(), _stats->_stateOfHealth);
            break;
        }

        case 0x356:
        case 0x405: {
            _stats->setVoltage(this->scaleValue(this->readSignedInt16(rx_message.data), 0.01), millis());
            _stats->setCurrent(this->scaleValue(this->readSignedInt16(rx_message.data + 2), 0.1), 1/*precision*/, millis());
            _stats->setTemperature(this->scaleValue(this->readSignedInt16(rx_message.data + 4), 0.1), millis());

            DTU_LOGD("voltage: %f current: %f temperature: %f",
                    _stats->getVoltage(), _stats->getChargeCurrent(), _stats->getTemperature().value());
            break;
        }

        case 0x35A: { // Victron protocol: Alarms and Warnings
            uint16_t alarmBits = rx_message.data[0];
            _stats->_alarmOverVoltage = this->getBit(alarmBits, 2);
            _stats->_alarmUnderVoltage = this->getBit(alarmBits, 4);
            _stats->_alarmOverTemperature = this->getBit(alarmBits, 6);

            alarmBits = rx_message.data[1];
            _stats->_alarmUnderTemperature = this->getBit(alarmBits, 0);
            _stats->_alarmOverTemperatureCharge = this->getBit(alarmBits, 2);
            _stats->_alarmUnderTemperatureCharge = this->getBit(alarmBits, 4);
            _stats->_alarmOverCurrentDischarge = this->getBit(alarmBits, 6);

            alarmBits = rx_message.data[2];
            _stats->_alarmOverCurrentCharge = this->getBit(alarmBits, 0);
            _stats->_alarmInternalFailure = this->getBit(alarmBits, 6);

            alarmBits = rx_message.data[3];
            _stats->_alarmCellImbalance = this->getBit(alarmBits, 0);

            DTU_LOGD("Alarms: %d %d %d %d %d %d %d %d %d %d",
                    _stats->_alarmOverVoltage,
                    _stats->_alarmUnderVoltage,
                    _stats->_alarmOverTemperature,
                    _stats->_alarmUnderTemperature,
                    _stats->_alarmOverTemperatureCharge,
                    _stats->_alarmUnderTemperatureCharge,
                    _stats->_alarmOverCurrentDischarge,
                    _stats->_alarmOverCurrentCharge,
                    _stats->_alarmInternalFailure,
                    _stats->_alarmCellImbalance);

            uint16_t warningBits = rx_message.data[4];
            _stats->_warningHighVoltage = this->getBit(warningBits, 2);
            _stats->_warningLowVoltage = this->getBit(warningBits, 4);
            _stats->_warningHighTemperature = this->getBit(warningBits, 6);

            warningBits = rx_message.data[5];
            _stats->_warningLowTemperature = this->getBit(warningBits, 0);
            _stats->_warningHighTemperatureCharge = this->getBit(warningBits, 2);
            _stats->_warningLowTemperatureCharge = this->getBit(warningBits, 4);
            _stats->_warningHighDischargeCurrent = this->getBit(warningBits, 6);

            warningBits = rx_message.data[6];
            _stats->_warningHighChargeCurrent = this->getBit(warningBits, 0);
            _stats->_warningInternalFailure = this->getBit(warningBits, 6);

            warningBits = rx_message.data[7];
            _stats->_warningCellImbalance = this->getBit(warningBits, 0);

            DTU_LOGD("Warnings: %d %d %d %d %d %d %d %d %d %d",
                    _stats->_warningHighVoltage,
                    _stats->_warningLowVoltage,
                    _stats->_warningHighTemperature,
                    _stats->_warningLowTemperature,
                    _stats->_warningHighTemperatureCharge,
                    _stats->_warningLowTemperatureCharge,
                    _stats->_warningHighDischargeCurrent,
                    _stats->_warningHighChargeCurrent,
                    _stats->_warningInternalFailure,
                    _stats->_warningCellImbalance);
            break;
        }

        case 0x35E:
        case 0x40A: {
            String manufacturer(reinterpret_cast<char*>(rx_message.data),
                    rx_message.data_length_code);

            if (manufacturer.isEmpty()) { break; }

            DTU_LOGD("Manufacturer: %s", manufacturer.c_str());

            _stats->setManufacturer(manufacturer);
            break;
        }

        case 0x35F: { // Victron protocol: BatteryInfo
            // bytes 2-3 are an internal build number (sent as value/100 and
            // value%100), not the SPBMS... firmware version, so they are
            // intentionally not exposed as a firmware version.
            _stats->_availableCapacity = this->readUnsignedInt16(rx_message.data + 4);

            DTU_LOGD("availableCapacity: %f Ah", _stats->_availableCapacity);
            break;
        }

        case 0x360: { // Victron protocol: Charging request
            _stats->_chargeImmediately = rx_message.data[0]; // 0xff requests charging.
            DTU_LOGD("chargeImmediately: %d", _stats->_chargeImmediately);
            break;
        }

        case 0x372: { // Victron protocol: BankInfo
            _stats->_moduleCountOnline = this->readUnsignedInt16(rx_message.data);
            _stats->_moduleCountBlockingCharge = this->readUnsignedInt16(rx_message.data + 2);
            _stats->_moduleCountBlockingDischarge = this->readUnsignedInt16(rx_message.data + 4);
            _stats->_moduleCountOffline = this->readUnsignedInt16(rx_message.data + 6);

            DTU_LOGD("moduleCountOnline: %d moduleCountBlockingCharge: %d "
                    "moduleCountBlockingDischarge: %d moduleCountOffline: %d",
                    _stats->_moduleCountOnline, _stats->_moduleCountBlockingCharge,
                    _stats->_moduleCountBlockingDischarge, _stats->_moduleCountOffline);
            break;
        }

        case 0x373: { // Victron protocol: CellInfo
            _stats->_cellMinMilliVolt = this->readUnsignedInt16(rx_message.data);
            _stats->_cellMaxMilliVolt = this->readUnsignedInt16(rx_message.data + 2);
            _stats->_cellMinTemperature = this->readUnsignedInt16(rx_message.data + 4) - 273;
            _stats->_cellMaxTemperature = this->readUnsignedInt16(rx_message.data + 6) - 273;

            DTU_LOGD("lowestCellMilliVolt: %d highestCellMilliVolt: %d "
                    "minimumCellTemperature: %f maximumCellTemperature: %f",
                    _stats->_cellMinMilliVolt, _stats->_cellMaxMilliVolt,
                    _stats->_cellMinTemperature, _stats->_cellMaxTemperature);
            break;
        }

        case 0x374: { // Victron protocol: Battery/Cell name (string) with "Lowest Cell Voltage"
            String cellMinVoltageName(reinterpret_cast<char*>(rx_message.data),
                    rx_message.data_length_code);

            if (cellMinVoltageName.isEmpty()) { break; }

            DTU_LOGD("cellMinVoltageName: %s", cellMinVoltageName.c_str());

            victronSetCellLabel(_stats->_cellMinVoltageName, cellMinVoltageName);
            break;
        }

        case 0x375: { // Victron protocol: Battery/Cell name (string) with "Highest Cell Voltage"
            String cellMaxVoltageName(reinterpret_cast<char*>(rx_message.data),
                    rx_message.data_length_code);

            if (cellMaxVoltageName.isEmpty()) { break; }

            DTU_LOGD("cellMaxVoltageName: %s", cellMaxVoltageName.c_str());

            victronSetCellLabel(_stats->_cellMaxVoltageName, cellMaxVoltageName);
            break;
        }

        case 0x376: { // Victron Protocol: Battery/Cell name (string) with "Minimum Cell Temperature"
            String cellMinTemperatureName(reinterpret_cast<char*>(rx_message.data),
                    rx_message.data_length_code);

            if (cellMinTemperatureName.isEmpty()) { break; }

            DTU_LOGD("cellMinTemperatureName: %s", cellMinTemperatureName.c_str());

            victronSetCellLabel(_stats->_cellMinTemperatureName, cellMinTemperatureName);
            break;
        }

        case 0x377: { // Victron Protocol: Battery/Cell name (string) with "Maximum Cell Temperature"
            String cellMaxTemperatureName(reinterpret_cast<char*>(rx_message.data),
                    rx_message.data_length_code);

            if (cellMaxTemperatureName.isEmpty()) { break; }

            DTU_LOGD("cellMaxTemperatureName: %s", cellMaxTemperatureName.c_str());

            victronSetCellLabel(_stats->_cellMaxTemperatureName, cellMaxTemperatureName);
            break;
        }

        case 0x378:
        case 0x41e: { // History: Charged / Discharged Energy
            _stats->_chargedEnergy = this->scaleValue(this->readUnsignedInt32(rx_message.data), 0.1);
            _stats->_dischargedEnergy = this->scaleValue(this->readUnsignedInt32(rx_message.data + 4), 0.1);

            DTU_LOGD("chargedEnergy: %f dischargedEnergy: %f",
                    _stats->_chargedEnergy, _stats->_dischargedEnergy);
            break;
        }

        case 0x379: { // BatterySize: Installed Ah
            _stats->_totalCapacity = this->readUnsignedInt16(rx_message.data);

            DTU_LOGD("totalCapacity: %f Ah", _stats->_totalCapacity);
            break;
        }

        case 0x380: { // Serialnumber - part 1
            String snPart1(reinterpret_cast<char*>(rx_message.data),
                    rx_message.data_length_code);

            if (snPart1.isEmpty() || !isgraph(static_cast<unsigned char>(snPart1.charAt(0)))) { break; }

            DTU_LOGD("snPart1: %s", snPart1.c_str());

            _stats->_serialPart1 = snPart1;
            _stats->updateSerial();
            break;
        }

        case 0x381: {  // Serialnumber - part 2
            String snPart2(reinterpret_cast<char*>(rx_message.data),
                    rx_message.data_length_code);

            if (snPart2.isEmpty() || !isgraph(static_cast<unsigned char>(snPart2.charAt(0)))) { break; }

            DTU_LOGD("snPart2: %s", snPart2.c_str());

            _stats->_serialPart2 = snPart2;
            _stats->updateSerial();
            break;
        }

        case 0x401: { // Pytes protocol: Highest/Lowest Cell Voltage
            _stats->_cellMaxMilliVolt = this->readUnsignedInt16(rx_message.data);
            _stats->_cellMinMilliVolt = this->readUnsignedInt16(rx_message.data + 2);
            pytesSetCellLabel(_stats->_cellMaxVoltageName, this->readUnsignedInt16(rx_message.data + 4));
            pytesSetCellLabel(_stats->_cellMinVoltageName, this->readUnsignedInt16(rx_message.data + 6));

            DTU_LOGD("lowestCellMilliVolt: %d highestCellMilliVolt: %d "
                    "cellMinVoltageName: %s cellMaxVoltageName: %s (raw: %s)",
                    _stats->_cellMinMilliVolt, _stats->_cellMaxMilliVolt,
                    _stats->_cellMinVoltageName.c_str(), _stats->_cellMaxVoltageName.c_str(),
                    hexBytes(rx_message.data, rx_message.data_length_code).c_str());
            break;
        }

        case 0x402: { // Pytes protocol: Highest/Lowest Cell Temperature
            _stats->_cellMaxTemperature = this->scaleValue(this->readSignedInt16(rx_message.data), 0.1);
            _stats->_cellMinTemperature = this->scaleValue(this->readSignedInt16(rx_message.data + 2), 0.1);
            pytesSetCellLabel(_stats->_cellMaxTemperatureName, this->readUnsignedInt16(rx_message.data + 4));
            pytesSetCellLabel(_stats->_cellMinTemperatureName, this->readUnsignedInt16(rx_message.data + 6));

            DTU_LOGD("minimumCellTemperature: %f maximumCellTemperature: %f "
                    "cellMinTemperatureName: %s cellMaxTemperatureName: %s (raw: %s)",
                    _stats->_cellMinTemperature, _stats->_cellMaxTemperature,
                    _stats->_cellMinTemperatureName.c_str(), _stats->_cellMaxTemperatureName.c_str(),
                    hexBytes(rx_message.data, rx_message.data_length_code).c_str());
            break;
        }

        case 0x403: { // Pytes protocol: Alarms and Warnings (part 1)
            uint32_t alarmBits1 = this->readUnsignedInt32(rx_message.data);
            uint32_t alarmBits2 = this->readUnsignedInt32(rx_message.data + 4);
            uint32_t mergedBits = alarmBits1 | alarmBits2;

            bool overVoltage = this->getBit(mergedBits, 0);
            bool highVoltage = this->getBit(mergedBits, 1);
            bool lowVoltage = this->getBit(mergedBits, 3);
            bool underVoltage = this->getBit(mergedBits, 4);
            bool overTemp = this->getBit(mergedBits, 8);
            bool highTemp = this->getBit(mergedBits, 9);
            bool lowTemp = this->getBit(mergedBits, 11);
            bool underTemp = this->getBit(mergedBits, 12);
            bool overCurrentDischarge = this->getBit(mergedBits, 17) || this->getBit(mergedBits, 18) || this->getBit(mergedBits, 24);
            bool overCurrentCharge = this->getBit(mergedBits, 19) || this->getBit(mergedBits, 20) || this->getBit(mergedBits, 29);
            bool highCurrentDischarge = this->getBit(mergedBits, 21);
            bool highCurrentCharge = this->getBit(mergedBits, 22);
            bool stateCharging = this->getBit(mergedBits, 26);
            bool stateDischarging = this->getBit(mergedBits, 27);

            _stats->_alarmOverVoltage = overVoltage;
            _stats->_alarmUnderVoltage = underVoltage;
            _stats->_alarmOverTemperature = stateDischarging && overTemp;
            _stats->_alarmUnderTemperature = stateDischarging && underTemp;
            _stats->_alarmOverTemperatureCharge = stateCharging && overTemp;
            _stats->_alarmUnderTemperatureCharge = stateCharging && underTemp;

            _stats->_alarmOverCurrentDischarge = overCurrentDischarge;
            _stats->_alarmOverCurrentCharge = overCurrentCharge;

            DTU_LOGD("Alarms: %d %d %d %d %d %d %d %d",
                    _stats->_alarmOverVoltage,
                    _stats->_alarmUnderVoltage,
                    _stats->_alarmOverTemperature,
                    _stats->_alarmUnderTemperature,
                    _stats->_alarmOverTemperatureCharge,
                    _stats->_alarmUnderTemperatureCharge,
                    _stats->_alarmOverCurrentDischarge,
                    _stats->_alarmOverCurrentCharge);

            _stats->_warningHighVoltage = highVoltage;
            _stats->_warningLowVoltage = lowVoltage;
            _stats->_warningHighTemperature = stateDischarging && highTemp;
            _stats->_warningLowTemperature = stateDischarging && lowTemp;
            _stats->_warningHighTemperatureCharge = stateCharging && highTemp;
            _stats->_warningLowTemperatureCharge = stateCharging && lowTemp;

            _stats->_warningHighDischargeCurrent = highCurrentDischarge;
            _stats->_warningHighChargeCurrent = highCurrentCharge;

            DTU_LOGD("Warnings: %d %d %d %d %d %d %d %d",
                    _stats->_warningHighVoltage,
                    _stats->_warningLowVoltage,
                    _stats->_warningHighTemperature,
                    _stats->_warningLowTemperature,
                    _stats->_warningHighTemperatureCharge,
                    _stats->_warningLowTemperatureCharge,
                    _stats->_warningHighDischargeCurrent,
                    _stats->_warningHighChargeCurrent);
            break;
        }

        case 0x404: { // Pytes protocol: SOC/SOH
            // soc (byte 0+1) isn't used here since it is generated with higher
            // precision in message 0x0409 below.
            _stats->_stateOfHealth = this->readUnsignedInt16(rx_message.data + 2);
            _stats->_chargeCycles = this->readUnsignedInt16(rx_message.data + 6);

            DTU_LOGD("soh: %d cycles: %d", _stats->_stateOfHealth, _stats->_chargeCycles);
            break;
        }

        case 0x406: { // Pytes protocol: alarms (part 2)
            // bit 9: module communication timeout, bits 2, 12-15, 20: self
            // checks (current sensor, SRAM, flash, PC/register, coding,
            // temperature) failed
            uint32_t alarmBits = this->readUnsignedInt32(rx_message.data);
            _stats->_alarmInternalFailure = (alarmBits & 0x0010F204) != 0;

            DTU_LOGD("internalFailure: %d (bits: %08x)", _stats->_alarmInternalFailure, alarmBits);
            break;
        }

        case 0x408: { // Pytes protocol: charge status
            // bytes 0/1: pack charge/discharge allowed (charge/discharge
            // current limit != 0). pack-level, unlike the per-module 0x40C.
            _stats->_chargeEnabled = rx_message.data[0] != 0;
            _stats->_dischargeEnabled = rx_message.data[1] != 0;
            // byte 2: force charge I, raised at SoC <= 8 %, cleared at
            // >= 12 %. byte 3: force charge II, raised at SoC < 10 %, cleared
            // at > 12 %. byte 5: modules blocking charge. same condition the
            // battery uses for the charge request in the Victron protocol
            // (0x360).
            _stats->_chargeImmediately = (rx_message.data[2] || rx_message.data[3]) && !rx_message.data[5];
            // byte 4: full charge request, raised while a module stays below
            // 97 % SoC for too long.
            _stats->_fullChargeRequest = static_cast<bool>(rx_message.data[4]);
            _stats->_moduleCountBlockingCharge = __builtin_popcount(rx_message.data[5]);
            _stats->_moduleCountBlockingDischarge = __builtin_popcount(rx_message.data[6]);

            DTU_LOGD("chargeEnabled: %d dischargeEnabled: %d chargeImmediately: %d fullChargeRequest: %d "
                    "moduleCountBlockingCharge: %d moduleCountBlockingDischarge: %d",
                    *_stats->_chargeEnabled, *_stats->_dischargeEnabled, _stats->_chargeImmediately, *_stats->_fullChargeRequest,
                    _stats->_moduleCountBlockingCharge, _stats->_moduleCountBlockingDischarge);
            break;
        }

        case 0x409: { // Pytes protocol: full mAh / remaining mAh
            _stats->_totalCapacity = this->scaleValue(this->readUnsignedInt32(rx_message.data), 0.001);
            _stats->_availableCapacity = this->scaleValue(this->readUnsignedInt32(rx_message.data + 4), 0.001);
            _stats->_capacityPrecision = 2;

            // a total capacity of 0 would result in an invalid SoC
            if (_stats->_totalCapacity <= 0) { break; }

            float soc = 100.0 * _stats->_availableCapacity / _stats->_totalCapacity;
            _stats->setSoC(soc, 2/*precision*/, millis());

            DTU_LOGD("soc: %.2f totalCapacity: %.2f Ah availableCapacity: %.2f Ah",
                    soc, _stats->_totalCapacity, _stats->_availableCapacity);
            break;
        }

        case 0x40b: { // Pytes protocol: online / offline module count
            _stats->_moduleCountOnline = this->readUnsignedInt8(rx_message.data + 6);
            _stats->_moduleCountOffline = this->readUnsignedInt8(rx_message.data + 7);

            DTU_LOGD("moduleCountOnline: %d moduleCountOffline: %d",
                    _stats->_moduleCountOnline, _stats->_moduleCountOffline);
            break;
        }

        case 0x40c: { // Pytes protocol: per-module charge/discharge enable
            // bytes 0-3: bitmask of modules with charging enabled (bit 0 =
            // module 1), bytes 4-7: modules with discharging enabled. Each bit
            // is the module's charge/discharge-allow flag that gates its FET
            // (charge cleared on over-voltage etc., discharge on under-voltage
            // etc.). 32-bit, so unlike 0x408 (8-bit) this covers all modules.
            // The blockingCharge/blockingDischarge module fields are derived
            // from these inverted.
            _stats->_moduleMaskChargeEnable = this->readUnsignedInt32(rx_message.data);
            _stats->_moduleMaskDischargeEnable = this->readUnsignedInt32(rx_message.data + 4);
            _stats->_moduleMasksValid = true;
            break;
        }

        case 0x40d: { // Pytes protocol: heating / balancing info
            // bytes 0-3: bitmask of the modules heating (bit 0 = module 1)
            _stats->_moduleMaskHeating = this->readUnsignedInt32(rx_message.data);
            _stats->_moduleCountHeating = __builtin_popcount(_stats->_moduleMaskHeating);

            // bytes 4-5: bitmask of the cells being balanced (bit 0 = cell 1),
            // combined over all modules (observed: 0x86E1 = 0x84E1 of module 1
            // OR 0x0240 of module 2 as reported via RS485), the module is not
            // reported. hence only active / not active is published.
            _stats->_balance = this->readUnsignedInt16(rx_message.data + 4);
            DTU_LOGD("balance: %d moduleCountHeating: %d (raw: %s)", _stats->_balance,
                    _stats->_moduleCountHeating,
                    hexBytes(rx_message.data, rx_message.data_length_code).c_str());
            break;
        }

        default:
            return; // do not update last update timestamp
            break;
    }

    _stats->setLastUpdate(millis());
}

// Pytes protocol only (the Victron protocol answers no requests): the cell
// data of a module is requested with 0x307 [group + 1, module number], the
// battery answers in its next cycle (once per second) and handles only one
// request at a time. the firmware version of a module is requested once with
// 0x19A60 (extended identifier) [module number (16 bit)].
void Provider::onLoop()
{
    // only request in the Pytes native protocol: the Victron protocol (the
    // default, used when an inverter is the CAN master) rejects these requests
    // in the battery firmware, so they are never answered. gating on a recent
    // Pytes-protocol frame also avoids injecting frames onto an inverter's bus.
    if (_lastPytesFrame == 0 || millis() - _lastPytesFrame > 5000) { return; }

    // no requests without recent data, e.g. while the battery is off.
    uint8_t moduleCount = std::min<uint8_t>(Stats::MaxModules,
            _stats->_moduleCountOnline + _stats->_moduleCountOffline);
    if (moduleCount == 0 || _stats->getAgeSeconds() > 10) { return; }

    // a request is completed by its answer, a lost answer is given up
    uint32_t now = millis();
    if (now - _lastRequest < 1000) { return; }
    if (_pendingModule != 0) {
        if (now - _lastRequest < 3000) { return; }
        _pendingModule = 0;
        if (_unansweredRequests < UINT8_MAX) { ++_unansweredRequests; }
        if (_unansweredRequests == 3) {
            DTU_LOGW("Module data requests are not answered, retrying every 10 minutes");
        }
    }

    // e.g. a battery firmware without support for these requests, or another
    // device on the bus interfering: only try now and then
    if (_unansweredRequests >= 3 && now - _lastRequest < 10 * 60 * 1000) { return; }
    _lastRequest = now;

    if (_nextModule > moduleCount) { _nextModule = 1; }
    uint8_t module = _nextModule;
    auto const& mod = _stats->_modules[module - 1];

    // the version is answered immediately, the cell data of the same module
    // is requested on the next turn
    if (mod.lastUpdate != 0 && mod.fwVersion.isEmpty() && _versionRequests[module - 1] < 3) {
        // count the attempt only if the request was actually sent: a dropped
        // transmit() must not use up the (few) attempts without a frame on the bus
        uint8_t data[8] = { module, 0 };
        if (transmit(0x19A60, data, sizeof(data), true)) { ++_versionRequests[module - 1]; }
        return;
    }

    // the battery talks in extended ids (its regular frames arrive extended),
    // so the request must be extended too, otherwise it is ignored. byte0 is
    // the CAN group + 1, fixed to 1 for the only supported group (default, 0).
    uint8_t data[8] = { 1, module }; // byte0 = group + 1, byte1 = module
    if (transmit(0x307, data, sizeof(data), true)) { _pendingModule = module; }
    _nextModule = module % moduleCount + 1;
}

// 0x4F0 + module number * 0x10 + n. n = 0: number of cells, 1-4: cell
// voltages [mV], 5-8: cell temperatures [0.1 °C], 4 cells per frame.
void Provider::onModuleCellFrame(uint32_t identifier, uint8_t const* data, uint8_t length)
{
    uint8_t module = (identifier - 0x4F0) >> 4;
    uint8_t n = (identifier - 0x4F0) & 0x0F;
    if (module < 1 || module > Stats::MaxModules || n > 8) { return; }

    auto& mod = _stats->_modules[module - 1];
    uint8_t* values = const_cast<uint8_t*>(data);

    // n == 0 starts a module's reply; track the subframes (0-8) received so a
    // lost one does not expose partial data as fresh (see the lastUpdate below)
    if (n == 0) {
        _cellFramesModule = module;
        _cellFrames = 1; // bit 0
        if (length >= 2) {
            mod.cellCount = std::min<uint16_t>(readUnsignedInt16(values), Stats::MaxCells);
        }
        return;
    }

    // a voltage/temperature frame without its n == 0 start (lost): ignore, the
    // reply stays incomplete and is retried
    if (module != _cellFramesModule) { return; }

    for (uint8_t i = 0; i < 4 && 2 * i + 1 < length; ++i) {
        uint8_t cell = ((n - 1) % 4) * 4 + i;
        if (n <= 4) {
            mod.cellMilliVolt[cell] = readUnsignedInt16(values + 2 * i);
        } else {
            mod.cellTemperature[cell] = readSignedInt16(values + 2 * i) / 10.0f;
        }
    }

    _cellFrames |= (1u << n);
    if (n < 8) { return; }

    // all subframes (cell count, 4 voltage, 4 temperature) must have arrived
    if (_cellFrames != 0x1FF) { return; }

    // a new module: announce its HASS device
    if (mod.lastUpdate == 0) { _hassIntegration->republish(); }

    mod.lastUpdate = millis();
    if (module == _pendingModule) { _pendingModule = 0; }
    _unansweredRequests = 0;

    DTU_LOGD("Module %u: %u cells, cell 1: %u mV %.1f °C", module, mod.cellCount,
            mod.cellMilliVolt[0], mod.cellTemperature[0]);
}

// 0x19A61: [length (16 bit), CRC16 (16 bit), module number (16 bit)],
// 0x19A62-0x19A65: version string, 8 characters each, zero padded
void Provider::onModuleVersionFrame(uint32_t identifier, uint8_t const* data, uint8_t length)
{
    uint8_t* values = const_cast<uint8_t*>(data);

    if (identifier == 0x19A61) {
        if (length < 6) { return; }
        uint16_t module = readUnsignedInt16(values + 4);
        _versionModule = (module >= 1 && module <= Stats::MaxModules) ? module : 0;
        _versionLength = std::min<uint16_t>(readUnsignedInt16(values), _versionBuffer.size());
        _versionBuffer.fill(0);
        _versionChunks = 0;
        return;
    }

    if (_versionModule == 0) { return; }

    size_t offset = (identifier - 0x19A62) * 8;
    memcpy(_versionBuffer.data() + offset, data, std::min<uint8_t>(length, 8));
    _versionChunks |= (1u << (identifier - 0x19A62)); // chunks 0-3

    if (identifier != 0x19A65) { return; }

    // the string is finalized on the last chunk (0x19A65), but a dropped chunk
    // before it would leave zeros in the buffer and store a truncated version
    // that is then never re-requested. require every chunk covering the length.
    uint8_t requiredChunks = (1u << ((_versionLength + 7) / 8)) - 1;
    if ((_versionChunks & requiredChunks) != requiredChunks) { _versionModule = 0; return; }

    String version;
    for (uint8_t i = 0; i < _versionLength && _versionBuffer[i] != 0; ++i) {
        if (!isprint(static_cast<unsigned char>(_versionBuffer[i]))) { break; }
        version += _versionBuffer[i];
    }
    version.trim();

    if (!version.isEmpty()) {
        _stats->_modules[_versionModule - 1].fwVersion = version;
        DTU_LOGI("Module %u: firmware %s", _versionModule, version.c_str());
        _hassIntegration->republish(); // the version is part of the HASS device
    }
    _versionModule = 0;
}

} // namespace Batteries::Pytes
