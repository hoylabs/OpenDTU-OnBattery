// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>

class WebApiGridChargerClass {
public:
    void init(AsyncWebServer& server, Scheduler& scheduler);

private:
    // Multi-charger endpoints
    void onStatusAll(AsyncWebServerRequest* request);
    void onStatus(AsyncWebServerRequest* request);
    void onAdminGet(AsyncWebServerRequest* request, uint8_t chargerId = 255);
    void onAdminPost(AsyncWebServerRequest* request, uint8_t chargerId = 255);
    void onLimitPost(AsyncWebServerRequest* request, uint8_t chargerId = 255);
    void onPowerPost(AsyncWebServerRequest* request, uint8_t chargerId = 255);

    // Backward compatibility endpoints
    void onAdminGetCompat(AsyncWebServerRequest* request);
    void onAdminPostCompat(AsyncWebServerRequest* request);
    void onLimitPostCompat(AsyncWebServerRequest* request);
    void onPowerPostCompat(AsyncWebServerRequest* request);

    uint8_t extractChargerId(AsyncWebServerRequest* request);

    AsyncWebServer* _server;
};

extern WebApiGridChargerClass WebApiGridCharger;
