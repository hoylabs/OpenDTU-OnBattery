// SPDX-License-Identifier: GPL-2.0-or-later
#include "SerialPortManager.h"
#include <HardwareSerial.h>
#include <esp_log.h>

#undef TAG
static const char* TAG = "serialPortManager";

SerialPortManagerClass SerialPortManager;

void SerialPortManagerClass::init()
{
    if (ARDUINO_USB_CDC_ON_BOOT != 1) {
        _ports[0] = "Serial Console";
        ESP_LOGI(TAG, "HW UART port 0 now in use by 'Serial Console'");
    }
}

std::optional<uint8_t> SerialPortManagerClass::allocatePort(std::string const& owner)
{
    for (size_t i = 0; i < _ports.size(); ++i) {
        if (_ports[i] != "") {
            ESP_LOGI(TAG, "HW UART %d already in use by '%s'", i, _ports[i].c_str());
            continue;
        }

        _ports[i] = owner;

        ESP_LOGI(TAG, "HW UART %d now in use by '%s'", i, owner.c_str());

        return i;
    }

    ESP_LOGE(TAG, "Cannot assign another HW UART port to '%s'", owner.c_str());
    _rejects.insert(owner);
    return std::nullopt;
}

void SerialPortManagerClass::freePort(std::string const& owner)
{
    for (size_t i = 0; i < _ports.size(); ++i) {
        if (_ports[i] != owner) { continue; }

        ESP_LOGI(TAG, "Freeing HW UART %d, owner was '%s'", i, owner.c_str());
        _ports[i] = "";
    }
}

SerialPortManagerClass::allocations_t SerialPortManagerClass::getAllocations() const
{
    allocations_t res;
    for (int8_t i = 0; i < _ports.size(); ++i) {
        res.push_back({i, _ports[i]});
    }
    for (auto const& reject : _rejects) {
        res.push_back({-1, reject});
    }
    return res;
}
