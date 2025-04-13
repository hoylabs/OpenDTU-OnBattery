// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <mutex>
#include <memory>
#include <TaskSchedulerDeclarations.h>
#include <solarcharger/Provider.h>
#include <solarcharger/smartbufferbatteries/Stats.h>
#include <espMqttClient.h>

namespace SolarChargers::SmartBufferBatteries {

class Provider : public ::SolarChargers::Provider {
public:
    Provider() = default;
    ~Provider() = default;

    bool init(bool verboseLogging) final;
    void deinit() final { return; };
    void loop() final { return; };
    std::shared_ptr<::SolarChargers::Stats> getStats() const final { return _stats; }
    std::shared_ptr<Stats> getStats() { return _stats; }

private:
    Provider(Provider const& other) = delete;
    Provider(Provider&& other) = delete;
    Provider& operator=(Provider const& other) = delete;
    Provider& operator=(Provider&& other) = delete;

    bool _verboseLogging = false;
    std::shared_ptr<Stats> _stats = std::make_shared<Stats>();
};

} // namespace SolarChargers::SmartBufferBatteries
