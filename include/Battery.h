// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>
#include <memory>

#include "BatteryStats.h"

class BatteryProvider {
    public:
        // returns true if the provider is ready for use, false otherwise
        virtual bool init() = 0;

        virtual void deinit() = 0;
        virtual void loop() = 0;
        virtual std::shared_ptr<BatteryStats> getStats() const = 0;
};

class BatteryClass {
    public:
        void init();
        void loop();
        void reload();

        std::shared_ptr<BatteryStats const> getStats() const;

    private:
        void mqttPublish();

        uint32_t _lastMqttPublish = 0;
        std::unique_ptr<BatteryProvider> _upProvider = nullptr;
};

extern BatteryClass Battery;
