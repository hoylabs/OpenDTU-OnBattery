// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>
#include <memory>

#include "BatteryStats.h"

class BatteryClass {
    public:
        BatteryClass()
            : _stats(std::make_shared<BatteryStats>()) { }

        void init();
        void loop();
        void reload();

        std::shared_ptr<BatteryStats const> getStats() const { return _stats; }

    private:
        void deinit();
        void mqttPublish();

        enum class Provider : int {
            None = -1,
            Pylontech = 0,
            JkBmsUart = 1,
            JkBmsTransceiver = 2,
        };

        Provider _provider = Provider::None;
        uint32_t _lastMqttPublish = 0;
        std::shared_ptr<BatteryStats const> _stats;
};

extern BatteryClass Battery;
