#pragma once

#include <Arduino.h>
#include "VeDirectData.h"
#include "VeDirectFrameHandler.h"

class VeDirectShuntController : public VeDirectFrameHandler<veShuntStruct> {
public:
    VeDirectShuntController() = default;

    void init(gpio_num_t rx, gpio_num_t tx, uint8_t hwSerialPort);

    using data_t = veShuntStruct;

private:
    bool processTextDataDerived(std::string const& name, std::string const& value) final;
};

extern VeDirectShuntController VeDirectShunt;
