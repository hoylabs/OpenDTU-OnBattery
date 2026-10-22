// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <vector>
#include <battery/Stats.h>

namespace Batteries::Pytes {

class Stats : public ::Batteries::Stats {
friend class Provider;

public:
    void getLiveViewData(JsonVariant& root) const final;
    void mqttPublish() const final;
    bool getImmediateChargingRequest() const { return _chargeImmediately; };

    static constexpr uint8_t MaxModules = 16;
    static constexpr uint8_t MaxCells = 16;

    size_t getLiveViewModuleCount() const final;
    bool getLiveViewModuleData(JsonVariant& root, size_t index, uint32_t since) const final;

    // modules that delivered cell data
    struct ModuleIdentity {
        uint8_t number;
        uint8_t cellCount;
        String fwVersion;
    };
    std::vector<ModuleIdentity> getModuleIdentities() const;

private:
    // per module data, requested if enabled (see Provider::onLoop()). the
    // modules are identified by their number only, the Pytes CAN protocol
    // has no module serials.
    struct Module {
        uint8_t cellCount = 0;
        std::array<uint16_t, MaxCells> cellMilliVolt = {};
        std::array<float, MaxCells> cellTemperature = {};
        String fwVersion;
        uint32_t lastUpdate = 0; // millis() of the last complete cell data
        mutable uint32_t lastPublish = 0; // lastUpdate of the published data
        mutable bool offlinePublished = false;
    };

    void mqttPublishModules() const;
    bool hasModuleData(uint8_t index) const;
    bool isModuleOnline(uint8_t index) const;

    void setLastUpdate(uint32_t ts) { _lastUpdate = ts; }
    void updateSerial() {
        if (!_serialPart1.isEmpty() && !_serialPart2.isEmpty()) {
            _serial = _serialPart1 + _serialPart2;
        }
    }

    String _serialPart1 = "";
    String _serialPart2 = "";

    float _chargeVoltageLimit;
    float _dischargeVoltageLimit;

    uint16_t _stateOfHealth;
    int _chargeCycles = -1;
    int _balance = -1;

    uint16_t _cellMinMilliVolt;
    uint16_t _cellMaxMilliVolt;
    float _cellMinTemperature;
    float _cellMaxTemperature;

    String _cellMinVoltageName;
    String _cellMaxVoltageName;
    String _cellMinTemperatureName;
    String _cellMaxTemperatureName;

    uint8_t _moduleCountOnline;
    uint8_t _moduleCountOffline;

    uint8_t _moduleCountBlockingCharge;
    uint8_t _moduleCountBlockingDischarge;
    int _moduleCountHeating = -1;

    float _totalCapacity;
    float _availableCapacity;
    uint8_t _capacityPrecision = 0; // decimal places

    float _chargedEnergy = -1;
    float _dischargedEnergy = -1;

    bool _alarmUnderVoltage;
    bool _alarmOverVoltage;
    bool _alarmOverCurrentCharge;
    bool _alarmOverCurrentDischarge;
    bool _alarmUnderTemperature;
    bool _alarmOverTemperature;
    bool _alarmUnderTemperatureCharge;
    bool _alarmOverTemperatureCharge;
    bool _alarmInternalFailure;
    bool _alarmCellImbalance;

    bool _warningLowVoltage;
    bool _warningHighVoltage;
    bool _warningHighChargeCurrent;
    bool _warningHighDischargeCurrent;
    bool _warningLowTemperature;
    bool _warningHighTemperature;
    bool _warningLowTemperatureCharge;
    bool _warningHighTemperatureCharge;
    bool _warningInternalFailure;
    bool _warningCellImbalance;

    bool _chargeImmediately;
    // 0x408 byte 0/1: pack charging/discharging allowed (CCL/DCL != 0). only
    // sent in the Pytes native protocol, so unset until the first 0x408 frame.
    std::optional<bool> _chargeEnabled;
    std::optional<bool> _dischargeEnabled;

    std::array<Module, MaxModules> _modules;
    uint32_t _moduleMaskHeating = 0;
    // 0x40C per-module charge/discharge enable (bit 0 = module 1), 32-bit so it
    // covers modules 1-32. The blockingCharge/blockingDischarge fields are
    // derived from these inverted (blocking = not enabled); 0x408 carries the
    // same for modules 1-8 only. _moduleMasksValid guards against reporting
    // "blocking" from the all-zero default before the first 0x40C frame.
    uint32_t _moduleMaskChargeEnable = 0;
    uint32_t _moduleMaskDischargeEnable = 0;
    bool _moduleMasksValid = false;
};

} // namespace Batteries::Pytes
