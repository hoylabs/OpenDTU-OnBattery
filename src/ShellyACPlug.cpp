// SPDX-License-Identifier: GPL-2.0-or-later
#include "Utils.h"
#include "ShellyACPlug.h"
#include "MessageOutput.h"
#include <WiFiClientSecure.h>
#include <base64.h>
#include <ESPmDNS.h>
#include "Configuration.h"
#include "Datastore.h"
#include "PowerMeter.h"
#include "Battery.h"

ShellyACPlugClass ShellyACPlug;


bool ShellyACPlugClass::init(Scheduler& scheduler)
{
    MessageOutput.printf("ShellyACPlug Initializing ... \r\n");
    _initialized = true;
    scheduler.addTask(_loopTask);
    _loopTask.setCallback([this] { loop(); });
    _loopTask.setIterations(TASK_FOREVER);
    _loopTask.setInterval(_period);
    _loopTask.enable();
    return false;
}

void ShellyACPlugClass::loop()
{
    const CONFIG_T& config = Configuration.get();
    verboselogging=config.Shelly.VerboseLogging;
    if (!config.Shelly.Enabled || !_initialized || !Configuration.get().PowerMeter.Enabled ) {
      return;
    }
    _loopTask.setInterval(_period);
    _acPower = PowerMeter.getPowerTotal();
    _SoC = Battery.getStats()->getSoC();
    _emergcharge = Battery.getStats()->getImmediateChargingRequest();
    _readpower = read_http("/rpc/Switch.GetStatus?id=0");
    if ((_acPower < config.Shelly.POWER_ON_threshold && powerstate && _SoC < config.Shelly.stop_batterysoc_threshold) || (_emergcharge && config.Shelly.Emergency_Charge_Enabled))
    {
        PowerON();
    }
    else if ((_acPower > config.Shelly.POWER_OFF_threshold && powerstate) || (_SoC >= config.Shelly.stop_batterysoc_threshold && powerstate))
    {
        PowerOFF();
    }
    if (verboselogging) {
        MessageOutput.print("[ShellyACPlug] Loop \r\n");
        MessageOutput.printf("[ShellyACPlug] %f W \r\n", _acPower );
        MessageOutput.printf("[ShellyACPlug] powerstate %d  \r\n", powerstate );
        MessageOutput.printf("[ShellyACPlug] Battery SoC %f  \r\n", _SoC);
        MessageOutput.printf("[ShellyACPlug] Verbrauch %f W \r\n", _readpower );
    }
}

void ShellyACPlugClass::PowerON()
{
    if (!send_http("/relay/0?turn=on"))
    {
        return;
    }
    powerstate=true;
    if (verboselogging) {
        MessageOutput.print("[ShellyACPlug] Power ON \r\n");
    }
}

void ShellyACPlugClass::PowerOFF()
{
    if (!send_http("/relay/0?turn=off"))
    {
        return;
    };
    powerstate=false;
    if (verboselogging) {
        MessageOutput.print("[ShellyACPlug] Power OFF \r\n");
    }
}

bool ShellyACPlugClass::send_http(String uri)
{
    auto guard = Configuration.getWriteGuard();
    auto& config = guard.getConfig();
    String url = config.Shelly.url;
    url += uri;
    HttpRequestConfig HttpRequest;
    strlcpy(HttpRequest.Url, url.c_str(), sizeof(HttpRequest.Url));
    HttpRequest.Timeout = 60;
    _HttpGetter = std::make_unique<HttpGetter>(HttpRequest);
    if (config.Shelly.VerboseLogging) {
        MessageOutput.printf("[ShellyACPlug] send_http Initializing: %s\r\n",url.c_str());
    }
    if (!_HttpGetter->init()) {
        MessageOutput.printf("[ShellyACPlug] INIT %s\r\n", _HttpGetter->getErrorText());
        return false;
    }
    if (!_HttpGetter->performGetRequest()) {
        MessageOutput.printf("[ShellyACPlug] GET %s\r\n", _HttpGetter->getErrorText());
        return false;
    }
    _HttpGetter = nullptr;
    return true;
}
float ShellyACPlugClass::read_http(String uri)
{
    const CONFIG_T& config = Configuration.get();
    String url = config.Shelly.url;
    url += uri;
    HttpRequestConfig HttpRequest;
    JsonDocument jsonResponse;
     strlcpy(HttpRequest.Url, url.c_str(), sizeof(HttpRequest.Url));
    HttpRequest.Timeout = 60;
    _HttpGetter = std::make_unique<HttpGetter>(HttpRequest);
    if (config.Shelly.VerboseLogging) {
        MessageOutput.printf("[ShellyACPlug] read_http Initializing: %s\r\n",url.c_str());
    }
    if (!_HttpGetter->init()) {
        MessageOutput.printf("[ShellyACPlug] INIT %s\r\n", _HttpGetter->getErrorText());
        return 0;
    }
    _HttpGetter->addHeader("Content-Type", "application/json");
    _HttpGetter->addHeader("Accept", "application/json");
    auto res = _HttpGetter->performGetRequest();
    if (!res) {
        MessageOutput.printf("[ShellyACPlug] GET %s\r\n", _HttpGetter->getErrorText());
        return 0;
    }
    auto pStream = res.getStream();
    if (!pStream) {
        MessageOutput.printf("Programmer error: HTTP request yields no stream");
        return 0;
    }

   const DeserializationError error = deserializeJson(jsonResponse, *pStream);
    if (error) {
        String msg("[ShellyACPlug] Unable to parse server response as JSON: ");
        MessageOutput.printf((msg + error.c_str()).c_str());
        return 0;
    }
    auto pathResolutionResult = Utils::getJsonValueByPath<float>(jsonResponse, "apower");
    if (!pathResolutionResult.second.isEmpty()) {
        MessageOutput.printf("[ShellyACPlug] second %s\r\n",pathResolutionResult.second.c_str());
    }

    _HttpGetter = nullptr;
    return pathResolutionResult.first;
}


