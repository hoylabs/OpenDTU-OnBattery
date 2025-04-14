// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>
#include <AsyncJson.h>

class WebApiGridChargerClass {
public:
    void init(AsyncWebServer& server, Scheduler& scheduler);
    void getJsonData(JsonVariant& root);
private:
    void onStatus(AsyncWebServerRequest* request);
    void onAdminGet(AsyncWebServerRequest* request);
    void onAdminPost(AsyncWebServerRequest* request);
    void onLimitPost(AsyncWebServerRequest* request);
    void onPowerPost(AsyncWebServerRequest* request);

    AsyncWebServer* _server;
};
