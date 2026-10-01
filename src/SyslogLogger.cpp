// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2026 Thomas Basler and others
 */
#include "SyslogLogger.h"
#include "Configuration.h"
#include "NetworkSettings.h"
#include "defaults.h"
#include <ESPmDNS.h>
#include <HardwareSerial.h>
#include <algorithm>

#undef TAG
static const char* TAG = "syslog";

// initial retry interval if the hostname could not be resolved. doubled after
// each failed attempt as resolving blocks the main loop until it times out.
static constexpr uint32_t RESOLVE_RETRY_INTERVAL_MS = 30 * 1000;

// re-resolve a successfully resolved hostname in this interval to pick up
// address changes (does not apply to numeric IP addresses)
static constexpr uint32_t RESOLVE_REFRESH_INTERVAL_MS = 10 * 60 * 1000;

SyslogLogger::SyslogLogger()
    : _loopTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&SyslogLogger::loop, this))
{
}

void SyslogLogger::init(Scheduler& scheduler)
{
    // PROCID change indicates a restart.
    _proc_id = String(esp_random(), HEX);

    scheduler.addTask(_loopTask);
    _loopTask.enable();
}

void SyslogLogger::updateSettings(const String&& hostname)
{
    auto& config = Configuration.get().Syslog;

    // Disable logger while it is reconfigured.
    disable();

    if (!config.Enabled) {
        ESP_LOGI(TAG, "Syslog not enabled");
        return;
    }

    _port = config.Port;
    _syslog_hostname = config.Hostname;
    if (_syslog_hostname.isEmpty()) {
        ESP_LOGW(TAG, "Hostname not configured");
        return;
    }

    ESP_LOGI(TAG, "Logging to %s!", _syslog_hostname.c_str());

    _header = ">1 - "; // RFC5424: Facility USER, severity INFO, version 1, NIL timestamp.
    _header += hostname;
    _header += " OpenDTU ";
    _header += _proc_id;
    // NIL values for message id and structured data
    _header += " - - ";

    // Enable logger.
    enable();
}

void SyslogLogger::write(const uint8_t* buffer, size_t size)
{
    std::lock_guard<std::mutex> lock(_mutex);
    // sending while the network is down fails and makes the Arduino core log
    // an error, which would in turn be sent to syslog and fail again.
    if (!_enabled || !isResolved() || !NetworkSettings.isConnected()) {
        return;
    }

    String header = "<";
    header += String(calculatePrival(1, buffer[0]));

    _udp.beginPacket(_address, _port);
    _udp.print(header);
    _udp.print(_header);

    for (int i = 0; i < size; i++) {
        uint8_t c = buffer[i];
        if (c != '\r' && c != '\n') {
            // Replace control and non-ASCII characters with '?'.
            _udp.write(c >= 0x20 && c < 0x7f ? c : '?');
        }
    }
    _udp.endPacket();
}

void SyslogLogger::disable()
{
    ESP_LOGI(TAG, "Disable");
    std::lock_guard<std::mutex> lock(_mutex);
    if (_enabled) {
        _enabled = false;
        _address = INADDR_NONE;
        _udp.stop();
    }
}

void SyslogLogger::enable()
{
    // Bind random source port.
    if (!_udp.begin(0)) {
        ESP_LOGE(TAG, "No sockets available");
        return;
    }

    std::lock_guard<std::mutex> lock(_mutex);

    // a numeric IP address needs no (repeated) resolution
    IPAddress address;
    _hostIsIp = address.fromString(_syslog_hostname);
    if (_hostIsIp) {
        _address = address;
    }

    _resolveNow = true;
    _resolveFailures = 0;
    _enabled = true;
}

IPAddress SyslogLogger::resolve()
{
    // MDNS.queryHost() appends ".local" itself, hence it can only resolve
    // plain hostnames. skip it otherwise, as a failed query takes 2 seconds.
    if (Configuration.get().Mdns.Enabled && _syslog_hostname.indexOf('.') < 0) {
        IPAddress address = MDNS.queryHost(_syslog_hostname); // INADDR_NONE if failed
        if (address != INADDR_NONE) {
            return address;
        }
    }

    // beginPacket() resolves the hostname using DNS and stores the resulting
    // address, which is then returned by remoteIP().
    if (!_udp.beginPacket(_syslog_hostname.c_str(), _port)) {
        return INADDR_NONE;
    }
    return _udp.remoteIP();
}

uint8_t SyslogLogger::calculatePrival(uint8_t facility, char errorCode)
{
    // ESP LOG ID's are two ahead of syslog ID's
    // e.g. ESP_LOG_ERROR (1) = Syslog ERROR 3
    if (errorCode == 'E') {
        return facility * 8 + ESP_LOG_ERROR + 2;
    } else if (errorCode == 'W') {
        return facility * 8 + ESP_LOG_WARN + 2;
    } else if (errorCode == 'D') {
        return facility * 8 + ESP_LOG_DEBUG + 2;
    } else if (errorCode == 'V') {
        return facility * 8 + ESP_LOG_VERBOSE + 2;
    }

    return facility * 8 + ESP_LOG_INFO + 2;
}

void SyslogLogger::loop()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_enabled || _hostIsIp || !NetworkSettings.isConnected()) {
        return;
    }

    uint32_t interval = RESOLVE_REFRESH_INTERVAL_MS;
    if (!isResolved() && _resolveFailures > 0) {
        uint32_t retryInterval = RESOLVE_RETRY_INTERVAL_MS << (_resolveFailures - 1);
        interval = std::min(retryInterval, RESOLVE_REFRESH_INTERVAL_MS);
    }
    if (!_resolveNow && millis() - _lastResolveAttempt < interval) {
        return;
    }
    _resolveNow = false;
    _lastResolveAttempt = millis();

    IPAddress address = resolve();
    if (address == INADDR_NONE) {
        // keep using the previously resolved address (if any)
        ESP_LOGW(TAG, "Could not resolve %s", _syslog_hostname.c_str());
        if (_resolveFailures < 6) {
            ++_resolveFailures;
        }
        return;
    }

    _resolveFailures = 0;

    if (address != _address) {
        ESP_LOGI(TAG, "Resolved %s to %s", _syslog_hostname.c_str(), address.toString().c_str());
        _address = address;
    }
}

SyslogLogger Syslog;
