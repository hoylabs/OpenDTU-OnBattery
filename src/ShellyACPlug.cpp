// SPDX-License-Identifier: GPL-2.0-or-later
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
    if (!config.Shelly.Enabled || !_initialized || !Configuration.get().PowerMeter.Enabled ) {
      return;
    }
    _loopTask.setInterval(_period);
    _acPower = PowerMeter.getPowerTotal();
    _SoC = Battery.getStats()->getSoC();
    _emergcharge = Battery.getStats()->getImmediateChargingRequest();
    if ((_acPower < config.Shelly.POWER_ON_threshold && !config.Shelly.POWER_ON && _SoC < config.Shelly.stop_batterysoc_threshold) || (_emergcharge && config.Shelly.Emergency_Charge_Enabled))
    {
        PowerON();
    }
    else if ((_acPower > config.Shelly.POWER_OFF_threshold && !config.Shelly.POWER_OFF) || (_SoC >= config.Shelly.stop_batterysoc_threshold && !config.Shelly.POWER_OFF))
    {
        PowerOFF();
    }
    bool _verboseLogging = config.Shelly.VerboseLogging;
    if (_verboseLogging) {
        MessageOutput.print("[ShellyACPlug] Loop \r\n");
        MessageOutput.printf("[ShellyACPlug] %f W \r\n", _acPower );
        MessageOutput.printf("[ShellyACPlug] ON: %d OFF: %d  \r\n", config.Shelly.POWER_ON, config.Shelly.POWER_OFF );
        MessageOutput.printf("[ShellyACPlug] Battery SoC %f  \r\n", _SoC);
    }
}

void ShellyACPlugClass::PowerON()
{
    CONFIG_T& config = Configuration.get();
    config.Shelly.POWER_ON = true;
    config.Shelly.POWER_OFF = false;
    Configuration.write();
    send_http("http://192.168.2.153/relay/0?turn=on");
    bool _verboseLogging = config.Shelly.VerboseLogging;
    if (_verboseLogging) {
        MessageOutput.print("[ShellyACPlug] Power ON \r\n");
    }
}

void ShellyACPlugClass::PowerOFF()
{
    CONFIG_T& config = Configuration.get();
    config.Shelly.POWER_ON = false;
    config.Shelly.POWER_OFF = true;
    Configuration.write();
    send_http("http://192.168.2.153/relay/0?turn=off");
    bool _verboseLogging = config.Shelly.VerboseLogging;
    if (_verboseLogging) {
        MessageOutput.print("[ShellyACPlug] Power OFF \r\n");
    }
}

void ShellyACPlugClass::send_http(String uri)
{
    JsonDocument doc;
    JsonObject obj = doc["http_request"].add<JsonObject>();
    obj["Url"] = uri;
    obj["Timeout"] = 60;
    HttpRequestConfig HttpRequest;
    JsonObject source_http_config = doc["http_request"];
    strlcpy(HttpRequest.Url, source_http_config["url"], sizeof(HttpRequest.Url));
    HttpRequest.Timeout = source_http_config["timeout"] | 60;
    _upHttpGetter = std::make_unique<HttpGetter>(HttpRequest);
    _upHttpGetter->init();
    _upHttpGetter->performGetRequest();
    MessageOutput.printf("[ShellyACPlug] Initializing:\r\n");
    MessageOutput.printf("[ShellyACPlug] %s\r\n", _upHttpGetter->getErrorText());
    _upHttpGetter = nullptr;
}
