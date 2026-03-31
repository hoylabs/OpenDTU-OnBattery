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
 * 2025.12.01 - 1.1 - added read mode ON_DEMAND
 */

#include <Utils.h>
#include <LittleFS.h>
#include <LogHelper.h>
#include "RuntimeData.h"
#include "PowerLimiter.h"


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

    // check if we need to write the runtime data, either it is 00:05 or on request
    if (_writeNow || getWriteTrigger()) {
        _writeNow = false;
        write(0); // no freeze time.
    }

    // check if we need to read the runtime data on request
    // for example, if some data is not available during startup
    if (_readNow) {
        _readNow = false;
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

    {
        std::lock_guard<std::mutex> lock(_mutex);

        // check minimum interval between writes (enforced only when freezeMinutes > 0)
        if ((freezeMinutes > 0) && (_writeEpoch != 0) && (difftime(nextEpoch, _writeEpoch) < 60 * freezeMinutes)) {
            return cleanExit(false, "Time interval between 2 write operations too short, skipping write");
        }

        // prepare the next write count
        nextCount = _writeCount + 1;

        JsonDocument doc;
        JsonObject info = doc["info"].to<JsonObject>();
        info["version"] = RUNTIME_VERSION;
        info["save_count"] = nextCount;
        info["save_epoch"] = nextEpoch;

        // Insert additional runtime data here and protect the shared data with a local mutex
        PowerLimiter.serializeRTD(doc["power_limiter"].to<JsonObject>());
        PowerLimiter.serializeATFtoRTD(doc.as<JsonVariant>());



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

        // commit the new state only after a successful write
        _writeVersion = RUNTIME_VERSION;
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
        _writeVersion = info["version"] | RUNTIME_VERSION;
        _writeCount = info["save_count"] | 0U;
        _writeEpoch = info["save_epoch"] | 0U;
    } // mutex is automatically released when lock goes out of this scope

    // deserialize additional runtime data here, prepare default values and protect the shared data with a local mutex
    // use mode == 0 for all data that can be initialized during startup
    if (mode == ReadMode::START_UP) {
        PowerLimiter.deserializeRTD(doc["power_limiter"]);
    } else {
        PowerLimiter.deserializeRTDtoATF(doc.as<JsonVariant>());
    }


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

    // Before we can convert the epoch to local time, we need to ensure we've received the correct time
    // from the time server. This may take some time after the system boots.
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
 * Returns true once a day between 00:05 - 00:10
 */
bool RuntimeClass::getWriteTrigger(void) {

    struct tm nowTime;
    if (!getLocalTime(&nowTime, 5)) {
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
