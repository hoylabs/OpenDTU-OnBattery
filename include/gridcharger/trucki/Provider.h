// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <atomic>
#include <gridcharger/Provider.h>
#include <gridcharger/trucki/Stats.h>
#include <HttpGetter.h>
#include <Utils.h>

namespace GridChargers::Trucki {

class Provider : public ::GridChargers::Provider {
public:
    bool init() final;
    void deinit() final;
    void loop() final;
    std::shared_ptr<::GridChargers::Stats> getStats() const final { return _stats; }

    bool getAutoPowerStatus() const final {
        return _dataCurrent.get<DataPointLabel::AcPowerSetpoint>().value_or(0) > 0.0f
            || _dataCurrent.get<DataPointLabel::DcPower>().value_or(0) > 0.0f;
    }

private:
    template<DataPointLabel L>
    void addStringToDataPoints(const JsonDocument& doc, const char* key)
    {
        if (!doc[key].is<std::string>()) {
            return;
        }

        _dataCurrent.add<L>(doc[key].as<std::string>());
    }

    template<DataPointLabel L>
    void addFloatToDataPoints(const JsonDocument& doc, const char* key)
    {
        std::optional<float> value;

        if (doc[key].is<float>()) {
            value = doc[key].as<float>();
        }

        if (doc[key].is<char const*>()) {
            value = Utils::getFromString<float>(doc[key].as<char const*>());
        }

        if (!value.has_value()) { return; }
        _dataCurrent.add<L>(*value);
    }

    static void pollingLoopHelper(void* context);
    void pollingLoop();
    void poll();

    TaskHandle_t _pollingTaskHandle = nullptr;
    std::atomic<bool> _pollingTaskDone = false;
    bool _stopPolling;
    mutable std::mutex _pollingMutex;
    std::condition_variable _cv;
    uint32_t _lastPoll = 0;

    std::unique_ptr<HttpGetter> _httpGetter;

    std::shared_ptr<Stats> _stats = std::make_shared<Stats>();

    DataPointContainer _dataCurrent;

    static constexpr int POLLING_INTERVAL_MS = 2000; // 2 seconds
    static constexpr int HTTP_REQUEST_TIMEOUT_MS = 50; // 50ms
};

} // namespace GridChargers::Trucki
