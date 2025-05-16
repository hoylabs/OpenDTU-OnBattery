#pragma once

#include <Arduino.h>
#include <esp_log.h>

#undef TAG

#define DTU_LOGE(fmt, ...) ESP_LOGE(TAG, "[%s] " fmt, SUBTAG, ##__VA_ARGS__)
#define DTU_LOGW(fmt, ...) ESP_LOGW(TAG, "[%s] " fmt, SUBTAG, ##__VA_ARGS__)
#define DTU_LOGI(fmt, ...) ESP_LOGI(TAG, "[%s] " fmt, SUBTAG, ##__VA_ARGS__)
#define DTU_LOGD(fmt, ...) ESP_LOGD(TAG, "[%s] " fmt, SUBTAG, ##__VA_ARGS__)
#define DTU_LOGV(fmt, ...) ESP_LOGV(TAG, "[%s] " fmt, SUBTAG, ##__VA_ARGS__)

class LogHelper {
public:
    static void dumpBytes(const char* tag, const char* subtag, const uint8_t* data, size_t len);
};
