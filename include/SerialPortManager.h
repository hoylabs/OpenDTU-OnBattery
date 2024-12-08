// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>
#include <utility>
#include <set>

class SerialPortManagerClass {
public:
    void init();

    std::optional<uint8_t> allocatePort(std::string const& owner);
    void freePort(std::string const& owner);

    using allocations_t = std::vector<std::pair<int8_t, std::string>>;
    allocations_t getAllocations() const;

private:
    // the amount of hardare UARTs available on supported ESP32 chips
    static size_t constexpr _num_controllers = 3;
    std::array<std::string, _num_controllers> _ports = { "" };
    std::set<std::string> _rejects;
};

extern SerialPortManagerClass SerialPortManager;
