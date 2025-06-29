// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2025 Thomas Basler and others
 */

#include "Utils.h"
#include "PinMapping.h"
#include <LittleFS.h>
#include <MD5Builder.h>

#undef TAG
static const char* TAG = "utils";

uint32_t Utils::getChipId()
{
    uint32_t chipId = 0;
    for (uint8_t i = 0; i < 17; i += 8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    return chipId;
}

uint64_t Utils::generateDtuSerial()
{
    uint32_t chipId = getChipId();
    uint64_t dtuId = 0;

    // Product category (char 1-4): 1 = Micro Inverter, 999 = Dummy
    dtuId |= 0x199900000000;

    // Year of production (char 5): 1 equals 2015 so hard code 8 = 2022
    dtuId |= 0x80000000;

    // Week of production (char 6-7): Range is 1-52 s hard code 1 = week 1
    dtuId |= 0x0100000;

    // Running Number (char 8-12): Derived from the ESP chip id
    for (uint8_t i = 0; i < 5; i++) {
        dtuId |= (chipId % 10) << (i * 4);
        chipId /= 10;
    }

    return dtuId;
}

int Utils::getTimezoneOffset()
{
    // see: https://stackoverflow.com/questions/13804095/get-the-time-zone-gmt-offset-in-c/44063597#44063597

    time_t gmt, rawtime = time(NULL);
    struct tm* ptm;

    struct tm gbuf;
    ptm = gmtime_r(&rawtime, &gbuf);

    // Request that mktime() looksup dst in timezone database
    ptm->tm_isdst = -1;
    gmt = mktime(ptm);

    return static_cast<int>(difftime(rawtime, gmt));
}

bool Utils::checkJsonAlloc(const JsonDocument& doc, const char* function, const uint16_t line)
{
    if (doc.overflowed()) {
        ESP_LOGE(TAG, "Alloc failed: %s, %" PRIu16 "", function, line);
        return false;
    }

    return true;
}

/// @brief Remove all files but the PINMAPPING_FILENAME
void Utils::removeAllFiles()
{
    auto root = LittleFS.open("/");
    auto file = root.getNextFileName();

    while (file != "") {
        if (file != PINMAPPING_FILENAME) {
            LittleFS.remove(file);
        }
        file = root.getNextFileName();
    }
}

String Utils::generateMd5FromFile(String file)
{
    if (!LittleFS.exists(file)) {
        return String();
    }

    File f = LittleFS.open(file, "r");
    if (!file) {
        return String();
    }

    MD5Builder md5;
    md5.begin();

    // Read the file in chunks to avoid using too much memory
    uint8_t buffer[512];

    while (f.available()) {
        size_t bytesRead = f.read(buffer, sizeof(buffer) / sizeof(buffer[0]));
        md5.add(buffer, bytesRead);
    }

    // Finalize and calculate the MD5 hash
    md5.calculate();

    f.close();

    return md5.toString();
}

void Utils::skipBom(File& f)
{
    // skip Byte Order Mask (BOM). valid JSON docs always start with '{' or '['.
    while (f.available() > 0) {
        int c = f.peek();
        if (c == '{' || c == '[') {
            break;
        }
        f.read();
    }
}

/* OpenDTU-OnBatter-specific utils go here: */
template<typename T>
std::optional<T> getFromString(char const* val);

template<>
std::optional<float> getFromString(char const* val)
{
    float res = 0;

    try {
        res = std::stof(val);
    }
    catch (std::invalid_argument const& e) {
        return std::nullopt;
    }

    return res;
}

template<typename T>
char const* getTypename();

template<>
char const* getTypename<float>() { return "float"; }

template<typename T>
std::pair<T, String> Utils::getJsonValueByPath(JsonDocument const& root, String const& path)
{
    size_t constexpr kErrBufferSize = 256;
    char errBuffer[kErrBufferSize];
    constexpr char delimiter = '/';
    int start = 0;
    int end = path.indexOf(delimiter);
    auto value = root.as<JsonVariantConst>();

    // NOTE: "Because ArduinoJson implements the Null Object Pattern, it is
    // always safe to read the object: if the key doesn't exist, it returns an
    // empty value."
    auto getNext = [&](String const& key) -> bool {
        // handle double forward slashes and paths starting or ending with a slash
        if (key.isEmpty()) { return true; }

        if (key[0] == '[' && key[key.length() - 1] == ']') {
            if (!value.is<JsonArrayConst>()) {
                snprintf(errBuffer, kErrBufferSize, "Cannot access non-array "
                        "JSON node using array index '%s' (JSON path '%s', "
                        "position %i)", key.c_str(), path.c_str(), start);
                return false;
            }

            auto idx = key.substring(1, key.length() - 1).toInt();
            value = value[idx];

            if (value.isNull()) {
                snprintf(errBuffer, kErrBufferSize, "Unable to access JSON "
                        "array index %li (JSON path '%s', position %i)",
                        idx, path.c_str(), start);
                return false;
            }

            return true;
        }

        value = value[key];

        if (value.isNull()) {
            snprintf(errBuffer, kErrBufferSize, "Unable to access JSON key "
                    "'%s' (JSON path '%s', position %i)",
                    key.c_str(), path.c_str(), start);
            return false;
        }

        return true;
    };

    while (end != -1) {
        if (!getNext(path.substring(start, end))) {
              return { T(), String(errBuffer) };
        }
        start = end + 1;
        end = path.indexOf(delimiter, start);
    }

    if (!getNext(path.substring(start))) {
        return { T(), String(errBuffer) };
    }

    if (value.is<T>()) {
        return { value.as<T>(), "" };
    }

    if (!value.is<char const*>()) {
        snprintf(errBuffer, kErrBufferSize, "Value '%s' at JSON path '%s' is "
                "neither a string nor of type %s", value.as<String>().c_str(),
                path.c_str(), getTypename<T>());
        return { T(), String(errBuffer) };
    }

    auto res = getFromString<T>(value.as<char const*>());
    if (!res.has_value()) {
        snprintf(errBuffer, kErrBufferSize, "String '%s' at JSON path '%s' cannot "
                "be converted to %s", value.as<String>().c_str(), path.c_str(),
                getTypename<T>());
        return { T(), String(errBuffer) };
    }

    return { *res, "" };
}

template std::pair<float, String> Utils::getJsonValueByPath(JsonDocument const& root, String const& path);

template <typename T>
std::optional<T> Utils::getNumericValueFromMqttPayload(char const* client,
        std::string const& src, char const* topic, char const* jsonPath)
{
    std::string logValue = src.substr(0, 32);
    if (src.length() > logValue.length()) { logValue += "..."; }

    if (strlen(jsonPath) == 0) {
        auto res = getFromString<T>(src.c_str());
        if (!res.has_value()) {
            ESP_LOGE(TAG, "[%s] Topic '%s': cannot parse payload '%s' as float", client, topic, logValue.c_str());
            return std::nullopt;
        }
        return res;
    }

    JsonDocument json;

    const DeserializationError error = deserializeJson(json, src);
    if (error) {
        ESP_LOGE(TAG, "[%s] Topic '%s': cannot parse payload '%s' as JSON", client, topic, logValue.c_str());
        return std::nullopt;
    }

    if (json.overflowed()) {
        ESP_LOGE(TAG, "[%s] Topic '%s': payload too large to process as JSON", client, topic);
        return std::nullopt;
    }

    auto pathResolutionResult = getJsonValueByPath<T>(json, jsonPath);
    if (!pathResolutionResult.second.isEmpty()) {
        ESP_LOGE(TAG, "[%s] Topic '%s': %s", client, topic, pathResolutionResult.second.c_str());
        return std::nullopt;
    }

    return pathResolutionResult.first;
}

template std::optional<float> Utils::getNumericValueFromMqttPayload(char const* client,
        std::string const& src, char const* topic, char const* jsonPath);

bool Utils::getEpoch(time_t* epoch, uint32_t ms /* = 20 */)
{
    uint32_t start = millis();
    while((millis()-start) <= ms) {
        time(epoch);
        if (*epoch > 1577836800) { /* epoch 2020-01-01T00:00:00 */
            return true;
        }
        delay(10);
    }
    return false;
}
