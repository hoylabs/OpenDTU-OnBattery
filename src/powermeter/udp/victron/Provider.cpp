// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2024 Holger-Steffen Stapf
 */
#include <powermeter/udp/victron/Provider.h>
#include <Arduino.h>
#include <WiFiUdp.h>
#include <MessageOutput.h>

namespace PowerMeters::Udp::Victron {

static constexpr unsigned int modbusPort = 502;  // local port to listen on
static WiFiUDP VictronUdp;

Provider::Provider(PowerMeterUdpVictronConfig const& cfg)
    : _cfg(cfg)
{
}

bool Provider::init()
{
    VictronUdp.begin(modbusPort);
    return true;
}

Provider::~Provider()
{
    VictronUdp.stop();
}

void Provider::sendModbusRequest()
{
    auto interval = _cfg.PollingIntervalMs;

    uint32_t currentMillis = millis();

    if (currentMillis - _lastRequest < interval) { return; }

    // TODO(schlimmchen): implement

    _lastRequest = currentMillis;
}

void Provider::parseModbusResponse()
{
    int packetSize = VictronUdp.parsePacket();
    if (!packetSize) { return; }

    uint8_t buffer[1024];
    int rSize = VictronUdp.read(buffer, 1024);

    // TODO(schlimmchen): implement
}

void Provider::loop()
{
    sendModbusRequest();
    parseModbusResponse();
}

} // namespace PowerMeters::Udp::Victron
