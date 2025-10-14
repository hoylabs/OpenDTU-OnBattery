// SPDX-License-Identifier: GPL-2.0-or-later

/* Runtime Data Management
 *
 * Read and write runtime data persistent on LittleFS
 * - The data is stored in JSON format
 * - The data is written during WebApp 'OTA firmware upgrade' and during Webapp 'Reboot'
 * - For security reasons such as 'unexpected power cycles' or 'physical resets', data is also written once a day at 00:05
 * - The data will not be written if the last write operation was less than one hour ago.
 * - Threadsave access to the data is provided by a mutex.
 *
 * Note: Runtime data must be added in the read() and write() methods.
 *       To avoid reenter deadlocks, do not call write() or read() from a locally locked mutex to save local data on demand!
 *
 * 2025.09.11 - 1.0 - first version
 */

#include <Utils.h>
#include <LittleFS.h>
#include <LogHelper.h>
#include "RuntimeData.h"


#undef TAG
static const char* TAG = "runtimedata";
static const char* SUBTAG = "Handler";


constexpr const char* RUNTIME_FILENAME = "/runtime.json";   // filename of the runtime data file
constexpr uint16_t RUNTIME_VERSION = 0;                     // version of the runtime data structure, prepared for future use


RuntimeClass RuntimeData {RUNTIME_VERSION}; // singleton instance


/*
 * Init the runtime data loop task
 */
void RuntimeClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&RuntimeClass::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.setInterval(60 * 1000); // every minute
    _loopTask.enable();
}


/*
 * The runtime data loop is called every minute
 */
void RuntimeClass::loop(void)
{
    // check whether it is time to write the runtime data but do not write if last write operation was less than 60 min ago
    if (getWriteTrigger()) { write(60); }
}


/*
 * Writes the runtime data to LittleFS file
 * freezeTime: Minimum necessary time [minutes] between now and last write operation
 */
bool RuntimeClass::write(uint16_t const freezeTime)
{
    auto cleanExit = [this](const bool writeOk, const char* text) -> bool {
        if (writeOk) {
            DTU_LOGI("%s", text);
        } else {
            DTU_LOGE("%s", text);
        }
        _writeOK = writeOk;
        return writeOk;
    };

    // we need the next local time before we can write the runtime data
    time_t nextEpoch;
    if (!Utils::getEpoch(&nextEpoch, 5)) { return cleanExit(false, "Local time not available, skipping write"); }
    uint16_t nextCount;

    { // prepare the next write count
        std::lock_guard<std::mutex> lock(_mutex);

        // we do not write more than once in a hour
        if ((_writeEpoch != 0) && (difftime(nextEpoch, _writeEpoch) < 60 * freezeTime)) {
            return cleanExit(false, "Time interval between 2 write operations too short, skipping write");
        }
        nextCount = _writeCount + 1;
    } // mutex is automatically released when lock goes out of this scope

    JsonDocument doc;
    JsonObject info = doc["info"].to<JsonObject>();
    info["version"] = RUNTIME_VERSION;
    info["save_count"] = nextCount;
    info["save_epoch"] = nextEpoch;

    // Insert additional runtime data here and protect the shared data with a local mutex



    if (!Utils::checkJsonAlloc(doc, __FUNCTION__, __LINE__)) {
        return cleanExit(false, "JSON alloc fault, skipping write");
    }

    File fRuntime = LittleFS.open(RUNTIME_FILENAME, "w");
    if (!fRuntime) { return cleanExit(false, "Failed to open file for writing"); }

    if (serializeJson(doc, fRuntime) == 0) {
        fRuntime.close();
        return cleanExit(false, "Failed to serialize to file");
    }

    fRuntime.close();

    { // commit the new state only after a successful write
        std::lock_guard<std::mutex> lock(_mutex);
        _writeVersion = RUNTIME_VERSION;
        _writeEpoch = nextEpoch;
        _writeCount = nextCount;
    } // mutex is automatically released when lock goes out of this scope

    return cleanExit(true, "Written to file");
}


/*
 * Read the runtime data from LittleFS file
 */
bool RuntimeClass::read(void)
{
    bool readOk = false;
    JsonDocument doc;

    // Note: We do not exit on read or allocation errors. In that case we need the default values
    File fRuntime = LittleFS.open(RUNTIME_FILENAME, "r", false);
    if (fRuntime) {
        Utils::skipBom(fRuntime);
        DeserializationError error = deserializeJson(doc, fRuntime);
        if (!error && Utils::checkJsonAlloc(doc, __FUNCTION__, __LINE__)) {
            readOk = true; // success of reading the runtime data
        }
    }

    JsonObject info = doc["info"];
    { // mutex is automatically released when lock goes out of this scope
        std::lock_guard<std::mutex> lock(_mutex);
        _writeVersion = info["version"] | RUNTIME_VERSION;
        _writeCount = info["save_count"] | 0U;
        _writeEpoch = info["save_epoch"] | 0U;
    } // mutex is automatically released when lock goes out of this scope

    // deserialize additional runtime data here, prepare default values and protect the shared data with a local mutex


    if (fRuntime) { fRuntime.close(); }
    if (readOk) {
        DTU_LOGI("Read successfully");
    } else {
        DTU_LOGE("Read fault, using default values");
    }
    _readOK = readOk;
    return readOk;
}


/*
 * Get the write counter
 */
uint16_t RuntimeClass::getWriteCount(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _writeCount;
}


/*
 * Get the write epoch time
 */
time_t RuntimeClass::getWriteEpochTime(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _writeEpoch;
}


/*
 * Get the write count and time as string
 * Format: "<count> / <dd>-<mon> <hh>:<mm>"
 * If epoch time and local time is not available the time is replaced by "no time"
 */
String RuntimeClass::getWriteCountAndTimeString(void) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    char buf[32] = "";
    struct tm time;

    // Check if time service is available before converting epoch to local time
    if ((_writeEpoch != 0) && (getLocalTime(&time, 5))) {
        localtime_r(&_writeEpoch, &time);
        strftime(buf, sizeof(buf), " / %d-%h %R", &time);
    } else {
        snprintf(buf, sizeof(buf), " / no time");
    }
    String ctString = String(_writeCount) + String(buf);
    return ctString;
}


/*
 * Returns true at the daily trigger time at 00:05
 * Note: This function is not protected by a mutex, but if it is only called by loop() and loop() is only executed on a single thread, we are safe
 */
bool RuntimeClass::getWriteTrigger(void) {
    struct tm actTime;
    if (getLocalTime(&actTime, 5)) {
        if ((actTime.tm_hour == 0) && (actTime.tm_min >= 5) && (actTime.tm_min <= 10)) {
            if (_lastTrigger == false) {
                _lastTrigger = true;
                return true;
            }
        } else {
            _lastTrigger = false;
        }
    }
    return false;
}
