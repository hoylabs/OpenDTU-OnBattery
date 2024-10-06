// SPDX-License-Identifier: GPL-2.0-or-later
#include "ShellyACPlug.h"
#include "MessageOutput.h"
#include <WiFiClientSecure.h>
#include <base64.h>
#include <ESPmDNS.h>

ShellyACPlug::~ShellyACPlug()
{
    _cv.notify_all();
}

bool ShellyACPlug::init()
{
    _upHttpGetter = std::make_unique<HttpGetter>(_cfg.HttpRequest);

    if (_upHttpGetter->init()) { return true; }

    MessageOutput.printf("[ShellyACPlug] Initializing HTTP getter failed:\r\n");
    MessageOutput.printf("[ShellyACPlug] %s\r\n", _upHttpGetter->getErrorText());

    _upHttpGetter = nullptr;

    return false;
}

void ShellyACPlug::loop()
{





    uint32_t constexpr stackSize = 3072;

}


