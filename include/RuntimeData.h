// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <TaskSchedulerDeclarations.h>
#include <mutex>
#include <cstdint>
#include <ctime>
#include <WString.h>

// Runtime data of all components that need to persist state across reboots.
// Unlike Configuration, this is not user-facing settings but internally
// derived state (e.g. "was the battery discharging when we lost power").
struct RUNTIME_DATA_T {
    struct {
        uint32_t Version;
        time_t   WriteEpoch;
        uint16_t WriteCount;
    } Meta;

    struct {
        bool FromStart;
        bool OneStopPerNightDone;
    } PowerLimiter;

    struct {
        // reserved for future battery runtime state
    } Battery;
};

class RuntimeDataClass {
public:
    void init(Scheduler& scheduler);
    bool read();
    bool write();
    RUNTIME_DATA_T const& get();
    String getWriteCountAndTimeString() const;

    class WriteGuard {
    public:
        WriteGuard();
        RUNTIME_DATA_T& getRuntimeData();
        ~WriteGuard();

    private:
        std::unique_lock<std::mutex> _lock;
        RUNTIME_DATA_T _before;
    };

    WriteGuard getWriteGuard();

private:
    void loop();
    bool getDailyWriteTrigger();

    Task _loopTask;
    bool _dirty = false;
    bool _lastTrigger = false;
};

extern RuntimeDataClass Runtime;
