// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <atomic>
#include <array>
#include <variant>
#include <memory>
#include <condition_variable>
#include <mutex>
#include <stdint.h>
#include <Configuration.h>
#include <HttpGetter.h>
#include <powermeter/Provider.h>

using Auth_t = HttpRequestConfig::Auth;
using Unit_t = PowerMeterHttpJsonValue::Unit;

namespace PowerMeters::Json::Http {

class Provider : public ::PowerMeters::Provider {
public:
    explicit Provider(PowerMeterHttpJsonConfig const& cfg)
        : _cfg(cfg) { }

    ~Provider();

    bool init() final;
    void loop() final;
    bool isDataValid() const final;

    using poll_result_t = std::variant<DataPointContainer, String>;
    poll_result_t poll();

private:
    static void pollingLoopHelper(void* context);
    std::atomic<bool> _taskDone;
    void pollingLoop();

    PowerMeterHttpJsonConfig const _cfg;

    uint32_t _lastPoll = 0;

    std::array<std::unique_ptr<HttpGetter>, POWERMETER_HTTP_JSON_MAX_VALUES> _httpGetters;

    TaskHandle_t _taskHandle = nullptr;
    bool _stopPolling;
    mutable std::mutex _pollingMutex;
    std::condition_variable _cv;
};

} // namespace PowerMeters::Json::Http
