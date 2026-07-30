// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Configuration.h"
#include "modbus/sunspec/SunSpecCommonModel1.h"
#include "modbus/sunspec/SunSpecInverterModel101_103.h"
#include "modbus/sunspec/SunSpecNameplateModel120.h"
#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <TaskSchedulerDeclarations.h>
#include <cstdint>
#include <array>
#include <memory>
#include <mutex>
#include <vector>

class InverterAbstract;

class ModbusServerClass {
public:
    ModbusServerClass();
    void init(Scheduler& scheduler);

    // (Re-)apply Configuration.get().ModbusServer: starts/stops/rebinds the TCP
    // listener as needed. Call after writing a changed Modbus config so
    // enable/port/inverter changes take effect without a reboot.
    void updateSettings();

private:
    static constexpr uint16_t kBase        = 40000;
    // SunSpec masters (e.g. Victron/dbus-fronius) open one persistent
    // connection per unit_id, not one shared connection for all - so this
    // must cover as many inverters as can be configured, not just a
    // handful of simultaneous clients.
    static constexpr uint8_t  kMaxClients  = INV_MAX_COUNT;
    static constexpr size_t   kMaxFrameLen = 260; // MBAP(6) + max PDU(254)
    // Refuse new clients below this free-heap floor so a full Modbus table
    // can't starve the web UI/MQTT of their own sockets/buffers. Internal
    // DRAM only - lwIP isn't PSRAM-backed.
    static constexpr uint32_t kMinFreeHeap = 20 * 1024;

    // Register map: SunS id (2 regs), then each model as a (2-reg ID/L
    // header + payload) block, then a 2-reg end marker. fillRegisters()
    // writes them in this same fixed order.
    static constexpr uint16_t kTotalRegs =
        2 // SunS
        + (2 + SunSpecCommonModel1::kLength)
        + (2 + SunSpecInverterModel101_103::kLength)
        + (2 + SunSpecNameplateModel120::kLength)
        + 2; // End marker

    struct Client {
        WiFiClient      tcp;
        std::vector<uint8_t> buf;
    };

    WiFiServer _server;
    std::array<Client, kMaxClients> _clients;
    Task _loopTask;
    std::mutex _mutex; // guards _server/_clients across threads

    void loop();
    void drainClient(Client& c);
    bool tryProcessFrame(Client& c);
    void handleReadRegs(WiFiClient& tcp, uint16_t tid, uint8_t unitId,
                        uint16_t startAddr, uint16_t regCount);
    void sendException(WiFiClient& tcp, uint16_t tid, uint8_t unitId,
                       uint8_t fc, uint8_t code);
    std::shared_ptr<InverterAbstract> unitIdToInverter(uint8_t unitId) const;
    void fillRegisters(uint16_t* regs, std::shared_ptr<InverterAbstract> const& inv, uint8_t unitId) const;
};

extern ModbusServerClass ModbusServer;
