// SPDX-License-Identifier: GPL-2.0-or-later
#include "SPIPortManager.h"
#include "MessageOutput.h"

SPIPortManagerClass SPIPortManager;
static constexpr char  TAG[] = "[SPIPortManager]";

void SPIPortManagerClass::init() {}

std::optional<uint8_t> SPIPortManagerClass::allocatePort(std::string const& owner)
{
    for (size_t i = 0; i < _ports.size(); ++i) {
        auto spiNum = i + _offset_spi_num;

        if (_ports[i] != "") {
            MessageOutput.printf("%s SPI%d already in use by '%s'\r\n", TAG, spiNum, _ports[i].c_str());
            continue;
        }

        _ports[i] = owner;

        MessageOutput.printf("%s SPI%d now in use by '%s'\r\n", TAG, spiNum, owner.c_str());

        return spiNum;
    }

    MessageOutput.printf("%s Cannot assign another SPI port to '%s'\r\n", TAG, owner.c_str());
    return std::nullopt;
}

void SPIPortManagerClass::freePort(std::string const& owner)
{
    for (size_t i = 0; i < _ports.size(); ++i) {
        if (_ports[i] != owner) { continue; }

        MessageOutput.printf("%s Freeing SPI%d, owner was '%s'\r\n", TAG, i + _offset_spi_num, owner.c_str());
        _ports[i] = "";
    }
}
