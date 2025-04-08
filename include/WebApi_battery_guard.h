// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>

class WebApiBatteryGuardClass {
public:
    void init(AsyncWebServer& server, Scheduler& scheduler);

private:
    void onMetaData(AsyncWebServerRequest* request);
    void onAdminGet(AsyncWebServerRequest* request);
    void onAdminPost(AsyncWebServerRequest* request);

    AsyncWebServer* _server;
};
