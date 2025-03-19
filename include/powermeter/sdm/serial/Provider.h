// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <SoftwareSerial.h>
#include <Configuration.h>
#include <powermeter/Provider.h>
#include <SDM.h>

namespace PowerMeters::Sdm::Serial {

class Provider : public ::PowerMeters::Provider {
public:
    enum class Phases {
        One,
        Three
    };

    Provider(Phases phases, PowerMeterSerialSdmConfig const& cfg)
        : _phases(phases)
        , _cfg(cfg) { }

    ~Provider();

    bool init() final;
    void loop() final;
    bool isDataValid() const final;

private:
    static void pollingLoopHelper(void* context);
    bool readValue(std::unique_lock<std::mutex>& lock, uint16_t reg, float& targetVar);
    std::atomic<bool> _taskDone;
    void pollingLoop();

    Phases _phases;
    PowerMeterSerialSdmConfig const _cfg;

    uint32_t _lastPoll = 0;

    std::unique_ptr<SoftwareSerial> _upSdmSerial = nullptr;
    std::unique_ptr<SDM> _upSdm = nullptr;

    TaskHandle_t _taskHandle = nullptr;
    bool _stopPolling;
    mutable std::mutex _pollingMutex;
    std::condition_variable _cv;
};

} // namespace PowerMeters::Sdm::Serial
