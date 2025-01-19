// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "ArduinoJson.h"
#include "Configuration.h"
#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>
#include <VeDirectMpptController.h>
#include <mutex>

class WebApiWsSolarChargerLiveClass {
public:
    WebApiWsSolarChargerLiveClass();
    void init(AsyncWebServer& server, Scheduler& scheduler);
    void reload();

private:
    void generateCommonJsonResponse(JsonVariant& root, bool fullUpdate);
    void onLivedataStatus(AsyncWebServerRequest* request);
    void onWebsocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);

    AsyncWebServer* _server;
    AsyncWebSocket _ws;
    AsyncAuthenticationMiddleware _simpleDigestAuth;

    uint32_t _lastFullPublish = 0;
    uint32_t _lastPublish = 0;

    std::mutex _mutex;

    Task _wsCleanupTask;
    void wsCleanupTaskCb();

    Task _sendDataTask;
    void sendDataTaskCb();
};
