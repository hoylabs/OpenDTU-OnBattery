// SPDX-License-Identifier: GPL-2.0-or-later

/* Runtime Data Management
 *
 * Read and write runtime data persistent on LittleFS.
 * - RuntimeData is the shared home for internally-derived state that components
 *   want to survive a reboot (as opposed to Configuration, which is user settings).
 * - The whole RUNTIME_DATA_T struct is known at compile time (like CONFIG_T), so
 *   there is no provider registration/dispatch: components just read/write their
 *   own section of the struct directly via get()/WriteGuard.
 * - read() is called once, synchronously, before any component's init() runs, so
 *   components have their persisted state available immediately.
 * - write() is only ever a no-op away from being called: it is gated on a dirty
 *   flag that WriteGuard sets by comparing the struct before/after mutation, so
 *   unchanged data is never rewritten (avoids flash wear and avoids bumping
 *   Meta.WriteEpoch when nothing actually changed).
 * - write() is triggered from WebApp 'OTA firmware upgrade' and 'Reboot' (via
 *   RestartHelper), and once a day at 00:05 as a safety net against unexpected
 *   power loss between those graceful triggers.
 *
 * 2025.09.11 - 1.0 - first version (generic provider/JSON model)
 * 2025.12.01 - 1.1 - added read mode ON_DEMAND and START_UP
 * 2026.05.06 - 1.2 - added InterfaceProviderRT interface and provider registration
 * 2026.07.06 - 2.0 - replaced provider/JSON-map model with a compile-time struct
 *                    (mirrors Configuration), dirty-gated writes, no eager startup read
 */

#include <Utils.h>
#include <LittleFS.h>
#include <esp_log.h>
#include <ArduinoJson.h>
#include "RuntimeData.h"

#undef TAG
static const char* TAG = "runtime";

static constexpr const char* RUNTIME_FILENAME = "/runtime.json";
static constexpr uint32_t RUNTIME_VERSION = 2;
static constexpr uint16_t WRITE_FREEZE_MINUTES = 10; // do not write more often than this

// JSON section/key names
static constexpr const char* META = "meta";
static constexpr const char* VERSION = "version";
static constexpr const char* WRITE_COUNT = "write_count";
static constexpr const char* WRITE_EPOCH = "write_epoch";
static constexpr const char* POWER_LIMITER = "power_limiter";
static constexpr const char* FROM_START = "from_start";
static constexpr const char* ONE_STOP_PER_NIGHT_DONE = "one_stop_per_night_done";

RuntimeDataClass Runtime; // singleton instance

static RUNTIME_DATA_T runtimeData;
static std::mutex sMutex;

void RuntimeDataClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    _loopTask.setCallback(std::bind(&RuntimeDataClass::loop, this));
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.setInterval(15 * 1000); // every 15 seconds
    _loopTask.enable();

    memset(&runtimeData, 0x0, sizeof(runtimeData));
}

// daily safety-net write, in case a reboot/OTA never happens gracefully
void RuntimeDataClass::loop(void)
{
    if (getDailyWriteTrigger()) { write(); }
}

bool RuntimeDataClass::getDailyWriteTrigger(void)
{
    struct tm nowTime;
    if (!getLocalTime(&nowTime, 1)) { return false; }

    if ((nowTime.tm_hour == 0) && (nowTime.tm_min >= 5) && (nowTime.tm_min <= 10)) {
        if (!_lastTrigger) {
            _lastTrigger = true;
            return true;
        }
    } else {
        _lastTrigger = false;
    }
    return false;
}

bool RuntimeDataClass::read()
{
    std::lock_guard<std::mutex> lock(sMutex);

    if (!LittleFS.exists(RUNTIME_FILENAME)) {
        ESP_LOGI(TAG, "No runtime data file, using default values");
        return false;
    }

    File fRuntime = LittleFS.open(RUNTIME_FILENAME, "r", false);
    if (!fRuntime) {
        ESP_LOGE(TAG, "Failed to open runtime data file");
        return false;
    }

    JsonDocument doc;
    Utils::skipBom(fRuntime);
    DeserializationError error = deserializeJson(doc, fRuntime);
    fRuntime.close();
    if (error || !Utils::checkJsonAlloc(doc, __FUNCTION__, __LINE__)) {
        ESP_LOGE(TAG, "Failed to parse runtime data file");
        return false;
    }

    JsonObject meta = doc[META];
    runtimeData.Meta.Version = meta[VERSION] | 0U;
    runtimeData.Meta.WriteCount = meta[WRITE_COUNT] | 0U;
    runtimeData.Meta.WriteEpoch = meta[WRITE_EPOCH].as<time_t>();

    JsonObject powerLimiter = doc[POWER_LIMITER];
    runtimeData.PowerLimiter.FromStart = powerLimiter[FROM_START] | false;
    runtimeData.PowerLimiter.OneStopPerNightDone = powerLimiter[ONE_STOP_PER_NIGHT_DONE] | false;

    ESP_LOGI(TAG, "Read from file");
    return true;
}

bool RuntimeDataClass::write()
{
    std::lock_guard<std::mutex> lock(sMutex);

    if (!_dirty) {
        ESP_LOGD(TAG, "No changes, skipping write");
        return true;
    }

    time_t nextEpoch;
    if (!Utils::getEpoch(&nextEpoch, 1)) {
        ESP_LOGW(TAG, "Local time not available, skipping write");
        return false;
    }

    if ((runtimeData.Meta.WriteEpoch != 0) &&
            (difftime(nextEpoch, runtimeData.Meta.WriteEpoch) < 60 * WRITE_FREEZE_MINUTES)) {
        ESP_LOGD(TAG, "Time interval too short, skipping write");
        return false;
    }

    runtimeData.Meta.Version = RUNTIME_VERSION;
    runtimeData.Meta.WriteEpoch = nextEpoch;
    runtimeData.Meta.WriteCount++;

    JsonDocument doc;
    JsonObject meta = doc[META].to<JsonObject>();
    meta[VERSION] = runtimeData.Meta.Version;
    meta[WRITE_COUNT] = runtimeData.Meta.WriteCount;
    meta[WRITE_EPOCH] = runtimeData.Meta.WriteEpoch;

    JsonObject powerLimiter = doc[POWER_LIMITER].to<JsonObject>();
    powerLimiter[FROM_START] = runtimeData.PowerLimiter.FromStart;
    powerLimiter[ONE_STOP_PER_NIGHT_DONE] = runtimeData.PowerLimiter.OneStopPerNightDone;

    if (!Utils::checkJsonAlloc(doc, __FUNCTION__, __LINE__)) {
        ESP_LOGE(TAG, "JSON alloc fault, skipping write");
        return false;
    }

    File fRuntime = LittleFS.open(RUNTIME_FILENAME, "w");
    if (!fRuntime) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return false;
    }

    if (serializeJson(doc, fRuntime) == 0) {
        fRuntime.close();
        ESP_LOGE(TAG, "Failed to serialize to file");
        return false;
    }
    fRuntime.close();

    _dirty = false;
    ESP_LOGI(TAG, "Write to file");
    return true;
}

RUNTIME_DATA_T const& RuntimeDataClass::get()
{
    return runtimeData;
}

String RuntimeDataClass::getWriteCountAndTimeString() const
{
    time_t epoch;
    uint16_t count;

    { // mutex is automatically released when lock goes out of this scope
        std::lock_guard<std::mutex> lock(sMutex);
        epoch = runtimeData.Meta.WriteEpoch;
        count = runtimeData.Meta.WriteCount;
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
    return String(count) + String(buf);
}

RuntimeDataClass::WriteGuard RuntimeDataClass::getWriteGuard()
{
    return WriteGuard();
}

RuntimeDataClass::WriteGuard::WriteGuard()
    : _lock(sMutex)
    , _before(runtimeData)
{
}

RUNTIME_DATA_T& RuntimeDataClass::WriteGuard::getRuntimeData()
{
    return runtimeData;
}

RuntimeDataClass::WriteGuard::~WriteGuard()
{
    if (memcmp(&_before, &runtimeData, sizeof(RUNTIME_DATA_T)) != 0) {
        Runtime._dirty = true;
    }
}
