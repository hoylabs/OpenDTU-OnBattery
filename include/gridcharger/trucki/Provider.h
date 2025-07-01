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

    void generalPowerControlLoop();

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

    static constexpr int POLLING_INTERVAL_MS = 1000; // 1 second
    static constexpr int HTTP_REQUEST_TIMEOUT_MS = 50; // 50ms

    void setChargerPowerAc(float powerAc);
    float _requestedPowerAc = 0;

    static void powerControlLoopHelper(void* context);
    void powerControlLoop();
    void sendPowerControlRequest();
    void parsePowerControlResponse();

    TaskHandle_t _powerControlTaskHandle = nullptr;
    std::atomic<bool> _powerControlTaskDone = false;
    bool _stopPowerControl;
    mutable std::mutex _powerControlMutex;
    std::condition_variable _powerControlCv;
    uint32_t _lastPowerControlRequestMillis = 0;

    static constexpr int POWER_CONTROL_INTERVAL_MS = 500; // 500ms

    std::shared_ptr<Stats> _stats = std::make_shared<Stats>();

    DataPointContainer _dataCurrent;

    uint32_t _lastPowerMeterUpdateReceivedMillis; // Timestamp of last seen power meter value
    uint32_t _autoModeBlockedTillMillis = 0;      // Timestamp to block running auto mode for some time

    bool _autoPowerEnabled = false;
};

} // namespace GridChargers::Trucki
