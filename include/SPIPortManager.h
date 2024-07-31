// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <SPI.h>
#include <array>
#include <optional>
#include <string>

/**
 * SPI# to SPI ID and SPI_HOST mapping
 *
 * ESP32-S3
 * | SPI # | SPI ID | SPI_HOST |
 * | 0     | 0      | 0        |
 * | 1     | 1      | 1        |
 * | 2     | 3      | 2        |
 *
 * ESP32
 * | SPI # | SPI ID | SPI_HOST |
 * | 0     | 1      | 0        |
 * | 1     | 2      | 1        |
 * | 2     | 3      | 2        |
 */

class SPIPortManagerClass {
public:
    void init();

    std::optional<uint8_t> allocatePort(std::string const& owner);
    void freePort(std::string const& owner);

private:
    // the amount of SPIs available on supported ESP32 chips
    // TODO(AndreasBoehm): this does not work as expected on non S3 ESPs
    static size_t constexpr _num_controllers = SOC_SPI_PERIPH_NUM;

    // TODO(AndreasBoehm): using a simple array like this does not work well when we want to support ESP32 and ESP32-S3
    std::array<std::string, _num_controllers> _ports = { "" };
};

extern SPIPortManagerClass SPIPortManager;
