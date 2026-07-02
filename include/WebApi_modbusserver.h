// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>

class WebApiModbusServerClass {
public:
    void init(AsyncWebServer& server, Scheduler& scheduler);

private:
    void onAdminGet(AsyncWebServerRequest* request);
    void onAdminPost(AsyncWebServerRequest* request);

    AsyncWebServer* _server;
};

extern WebApiModbusServerClass WebApiModbusServer;
