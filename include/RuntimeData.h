// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <ArduinoJson.h>
#include <TaskSchedulerDeclarations.h>
#include <mutex>
#include <atomic>


class RuntimeClass {
public:
    explicit RuntimeClass(uint16_t version) : _writeVersion(version) {};
    ~RuntimeClass() = default;
    RuntimeClass(const RuntimeClass&) = delete;
    RuntimeClass& operator=(const RuntimeClass&) = delete;
    RuntimeClass(RuntimeClass&&) = delete;
    RuntimeClass& operator=(RuntimeClass&&) = delete;

    void init(Scheduler& scheduler);
    bool read(void);
    bool write(void);

    uint16_t getWriteCount(void) const;
    time_t getWriteEpochTime(void) const;
    bool getReadState(void) const { return _readOK; }
    bool getWriteState(void) const { return _writeOK; }
    String getWriteCountAndTimeString(void) const;

private:
    void loop(void);
    bool getWriteTrigger(void);

    Task _loopTask;
    std::atomic<bool> _readOK = false;      // true if the last read operation was successful
    std::atomic<bool> _writeOK = false;     // true if the last write operation was successful
    mutable std::mutex _mutex;              // to protect the shared data below
    bool _lastTrigger = false;              // auxiliary value to prevent multiple triggering on the same day
    uint16_t _writeVersion = 0;             // shared data: version of the runtime data
    uint16_t _writeCount = 0;               // shared data: number of write operations
    time_t _writeEpoch = 0;                 // shared data: epoch time when the data was written
};

extern RuntimeClass RuntimeData;
