// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <memory>
#include <condition_variable>
#include "HttpGetter.h"
#include "Configuration.h"
#include <TaskSchedulerDeclarations.h>
#include <stdint.h>
#include "PowerMeterProvider.h"


#include <atomic>
#include <array>
#include <variant>
#include <mutex>


class ShellyACPlug : public PowerMeterProvider  {
public:
    explicit ShellyACPlug(ShellyACPlugConfig const& cfg)
        : _cfg(cfg) { }
    ~ShellyACPlug();
    bool init();
    void loop();

private:
    std::unique_ptr<HttpGetter> _upHttpGetter;
    ShellyACPlugConfig const& _cfg;
    std::condition_variable _cv;
};
