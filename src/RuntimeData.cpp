// SPDX-License-Identifier: GPL-2.0-or-later

/* Runtime Data Management
 *
 * Read and write runtime data persistent on LittleFS
 * - The data is stored in JSON format
 * - The data is written during WebApp 'OTA firmware upgrade' and during Webapp 'Reboot'
 * - For security reasons such as 'unexpected power cycles' or 'physical resets', data is also written once a day at 00:05
 * - The data will not be written if the last write operation was less than one hour ago. ('OTA firmware upgrade' and 'Reboot')
 * - Threadsave access to the data is provided by a mutex.
 *
 * How to use:
 *  - Runtime data must be added in the read() and write() methods.
 *  - To avoid reenter deadlocks, do not call write() or read() from a locally locked mutex to save locally data on demand!
 *  - Use requestWriteOnNextTaskLoop() and requestReadOnNextTaskLoop() to avoid deadlocks if you want to handle locally data on demand.
 *
 * 2025.09.11 - 1.0 - first version
 * 2025.12.01 - 1.1 - added read mode ON_DEMAND and START_UP
 */

#include <Utils.h>
#include <LittleFS.h>
#include <esp_log.h>
#include <ArduinoJson.h>
#include "RuntimeData.h"


#undef TAG
static const char* TAG = "runtime";


constexpr const char* RUNTIME_FILENAME = "/runtime.json";   // filename of the runtime data file
constexpr uint16_t RUNTIME_VERSION = 1;                     // version prepared for future migration support


RuntimeClass RuntimeData; // singleton instance


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

    // check if we need to write the runtime data, either it is 00:05 or on request
    if (_writeNow.exchange(false) || getWriteTrigger()) {
        write(0); // no freeze time.
    }

    // check if we need to read the runtime data on request
    // for example, if some data is not available during startup
    if (_readNow.exchange(false)) {
        read(ReadMode::ON_DEMAND); // read data that can be read on demand
    }
}


/*
 * Writes the runtime data to LittleFS file
 * freezeMinutes: Minimum necessary time [minutes] between now and last write operation
 */
bool RuntimeClass::write(uint16_t const freezeMinutes)
{
    auto cleanExit = [this](const bool writeOk, const char* text) -> bool {
        if (writeOk) {
            ESP_LOGI(TAG,"%s", text);
        } else {
            ESP_LOGE(TAG,"%s", text);
        }
        _writeOK.store(writeOk);
        return writeOk;
    };

    // we need a valid epoch time before we can write the runtime data
    time_t nextEpoch;
    if (!Utils::getEpoch(&nextEpoch, 1)) { return cleanExit(false, "Local time not available, skipping write"); }
    uint16_t nextCount;

    {
        std::lock_guard<std::mutex> lock(_mutex);

        // check minimum interval between writes (enforced only when freezeMinutes > 0)
        if ((freezeMinutes > 0) && (_writeEpoch != 0) && (difftime(nextEpoch, _writeEpoch) < 60 * freezeMinutes)) {
            return cleanExit(false, "Time interval too short, skipping write");
        }

        // prepare the next write count
        nextCount = _writeCount + 1;

    } // mutex is automatically released when lock goes out of this scope

    // prepare the JSON document and store the runtime data in it is done outside the
    // mutex protection to minimize the time the mutex is locked.
    JsonDocument doc;
    JsonObject info = doc["info"].to<JsonObject>();
    info["version"] = RUNTIME_VERSION;
    info["save_count"] = nextCount;
    info["save_epoch"] = nextEpoch;

    // serialize additional runtime data here
    // make sure the additional data remains under its own mutex protection.
    // todo: serialize additional runtime data

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

    {
        std::lock_guard<std::mutex> lock(_mutex);

        // commit the new state only after a successful write
        _fileVersion = RUNTIME_VERSION;
        _writeEpoch = nextEpoch;
        _writeCount = nextCount;

    } // mutex is automatically released when lock goes out of this scope

    return cleanExit(true, "Written to file");
}


/*
 * Read the runtime data from LittleFS file
 * mode = START_UP: read data that can be initialized during startup
 * mode = ON_DEMAND: read data that can not be read during startup
 */
bool RuntimeClass::read(ReadMode const mode)
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
        _fileVersion = info["version"] | 0U; // 0 means no file available and runtime data is not valid
        _writeCount = info["save_count"] | 0U;
        _writeEpoch = info["save_epoch"] | 0U;
    } // mutex is automatically released when lock goes out of this scope

    // deserialize additional runtime data here, prepare default values and protect the shared data with a mutex
    // use ReadMode::START_UP for all data that can be initialized during startup
    if (mode == ReadMode::START_UP) {
        ; // todo: deserialize additional runtime data that can be initialized during startup
    } else {
        ; // todo: deserialize additional runtime data that can not be initialized during startup
    }


    if (fRuntime) { fRuntime.close(); }
    if (readOk) {
        ESP_LOGI(TAG, "Read successfully");
    } else {
        ESP_LOGE(TAG, "Read fault, using default values");
    }
    _readOK.store(readOk);
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
    time_t epoch;
    uint16_t count;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        epoch = _writeEpoch;
        count = _writeCount;
    } // mutex is automatically released when lock goes out of this scope

    char buf[32] = "";
    struct tm time;

    // Before we can convert the epoch to local time, we need to ensure we've received the correct time
    // from the time server. This may take some time after the system startup.
    if ((epoch != 0) && (getLocalTime(&time, 1))) {
        localtime_r(&epoch, &time);
        strftime(buf, sizeof(buf), " / %d-%h %R", &time);
    } else {
        snprintf(buf, sizeof(buf), " / no time");
    }
    String ctString = String(count) + String(buf);
    return ctString;
}


/*
 * Returns true once a day between 00:05 - 00:10
 */
bool RuntimeClass::getWriteTrigger(void) {

    struct tm nowTime;
    if (!getLocalTime(&nowTime, 1)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    if ((nowTime.tm_hour == 0) && (nowTime.tm_min >= 5) && (nowTime.tm_min <= 10)) {
        if (_lastTrigger == false) {
            _lastTrigger = true;
            return true;
        }
    } else {
        _lastTrigger = false;
    }
    return false;
}
