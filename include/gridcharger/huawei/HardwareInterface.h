// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

namespace GridCharger::Huawei {

class HardwareInterface {
public:
    virtual bool init() = 0;

    virtual uint8_t getErrorCode(bool clear) = 0;

    enum class Setting : uint8_t {
        OnlineVoltage = 0,
        OfflineVoltage = 1,
        OnlineCurrent = 3,
        OfflineCurrent = 4
    };
    virtual void setParameter(Setting setting, float val) = 0;

    enum class Property : uint8_t {
        InputPower = 0x70,
        InputFrequency = 0x71,
        InputCurrent = 0x72,
        OutputPower = 0x73,
        Efficiency = 0x74,
        OutputVoltage = 0x75,
        OutputCurrentMax = 0x76,
        InputVoltage = 0x78,
        OutputTemperature = 0x7F,
        InputTemperature = 0x80,
        OutputCurrent = 0x81
    };
    using property_t = std::pair<float, uint32_t>; // value and timestamp
    virtual property_t getParameter(Property prop) = 0;
};

} // namespace GridCharger::Huawei
