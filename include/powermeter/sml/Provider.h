// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <list>
#include <mutex>
#include <optional>
#include <stdint.h>
#include <Arduino.h>
#include <HTTPClient.h>
#include <Configuration.h>
#include <powermeter/Provider.h>
#include <sml.h>

namespace PowerMeters::Sml {

class Provider : public ::PowerMeters::Provider {
protected:
    explicit Provider(char const* user)
        : _user(user) { }

    void reset();
    void processSmlByte(uint8_t byte);

private:
    std::string _user;

    DataPointContainer _dataInFlight;

    using OBISHandler = struct {
        uint8_t const OBIS[6];
        void (*decoder)(float&);
        DataPointLabel target;
        char const* name;
    };

    const std::list<OBISHandler> smlHandlerList{
        {{0x01, 0x00, 0x10, 0x07, 0x00, 0xff}, &smlOBISW, DataPointLabel::PowerTotal, "PowerTotal"},
        {{0x01, 0x00, 0x24, 0x07, 0x00, 0xff}, &smlOBISW, DataPointLabel::PowerL1, "PowerL1"},
        {{0x01, 0x00, 0x38, 0x07, 0x00, 0xff}, &smlOBISW, DataPointLabel::PowerL2, "PowerL2"},
        {{0x01, 0x00, 0x4c, 0x07, 0x00, 0xff}, &smlOBISW, DataPointLabel::PowerL3, "PowerL3"},
        {{0x01, 0x00, 0x20, 0x07, 0x00, 0xff}, &smlOBISVolt, DataPointLabel::VoltageL1, "VoltageL1"},
        {{0x01, 0x00, 0x34, 0x07, 0x00, 0xff}, &smlOBISVolt, DataPointLabel::VoltageL2, "VoltageL2"},
        {{0x01, 0x00, 0x48, 0x07, 0x00, 0xff}, &smlOBISVolt, DataPointLabel::VoltageL3, "VoltageL3"},
        {{0x01, 0x00, 0x1f, 0x07, 0x00, 0xff}, &smlOBISAmpere, DataPointLabel::CurrentL1, "CurrentL1"},
        {{0x01, 0x00, 0x33, 0x07, 0x00, 0xff}, &smlOBISAmpere, DataPointLabel::CurrentL2, "CurrentL2"},
        {{0x01, 0x00, 0x47, 0x07, 0x00, 0xff}, &smlOBISAmpere, DataPointLabel::CurrentL3, "CurrentL3"},
        {{0x01, 0x00, 0x01, 0x08, 0x00, 0xff}, &smlOBISWh, DataPointLabel::Import, "Import"},
        {{0x01, 0x00, 0x02, 0x08, 0x00, 0xff}, &smlOBISWh, DataPointLabel::Export, "Export"}
    };
};

} // namespace PowerMeters::Sml
