// SPDX-License-Identifier: GPL-2.0-or-later
#include "modbus/ModbusServer.h"
#include "Configuration.h"
#include <Hoymiles.h>
#include <functional>
#include <memory>
#include <Arduino.h>
#include <esp_log.h>

#undef TAG
static const char* TAG = "ModbusServer";

ModbusServerClass ModbusServer;

// ---------------------------------------------------------------------------

ModbusServerClass::ModbusServerClass()
    : _server(0)
    , _loopTask(20 * TASK_MILLISECOND, TASK_FOREVER, std::bind(&ModbusServerClass::loop, this))
{
}

void ModbusServerClass::init(Scheduler& scheduler)
{
    scheduler.addTask(_loopTask);
    updateSettings();
}

void ModbusServerClass::updateSettings()
{
    _loopTask.disable();
    for (auto& c : _clients) {
        if (c.tcp) c.tcp.stop();
        c.buf.clear();
    }
    _server.end();

    auto const& cfg = Configuration.get().ModbusServer;
    if (!cfg.Enabled) {
        ESP_LOGI(TAG, "SunSpec/Modbus TCP server disabled");
        return;
    }
    _server = WiFiServer(cfg.Port);
    _server.begin();
    _loopTask.enable();
    ESP_LOGI(TAG, "SunSpec/Modbus TCP listening on port %u", cfg.Port);
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------

void ModbusServerClass::loop()
{
    for (auto& c : _clients) {
        if (!c.tcp || !c.tcp.connected()) {
            WiFiClient n = _server.accept();
            if (n) {
                // Accepted sockets get no send/receive timeout by default in
                // this framework (unlike client-initiated connect()), so a
                // stalled peer could otherwise block this task's write()
                // indefinitely. 1s is the finest granularity setTimeout()
                // offers; write() below drops the client if that's not
                // enough for it to drain a reply.
                n.setTimeout(1);
                c.tcp = n;
                c.buf.clear();
                ESP_LOGD(TAG, "Client connected from %s", n.remoteIP().toString().c_str());
            }
        }
    }
    for (auto& c : _clients) {
        if (c.tcp && c.tcp.connected()) drainClient(c);
    }
}

// ---------------------------------------------------------------------------
// Non-blocking read: append available bytes, process complete frames
// ---------------------------------------------------------------------------

void ModbusServerClass::drainClient(Client& c)
{
    // Read all currently available bytes — never block
    int avail = c.tcp.available();
    while (avail-- > 0) {
        int b = c.tcp.read();
        if (b < 0) break;
        if (c.buf.size() < kMaxFrameLen)
            c.buf.push_back((uint8_t)b);
        else {
            // Oversized frame — discard and reset
            c.buf.clear();
            c.tcp.flush();
            return;
        }
    }

    // Process as many complete frames as the buffer holds
    while (tryProcessFrame(c)) {}
}

bool ModbusServerClass::tryProcessFrame(Client& c)
{
    // Need at least 6-byte MBAP header
    if (c.buf.size() < 6) return false;

    uint16_t pduLen = (static_cast<uint16_t>(c.buf[4]) << 8) | c.buf[5];
    size_t   total  = 6u + pduLen; // MBAP + PDU

    if (pduLen < 2 || pduLen > 254) {
        c.buf.clear(); // malformed
        return false;
    }

    // Full frame not yet buffered
    if (c.buf.size() < total) return false;

    uint16_t tid    = (static_cast<uint16_t>(c.buf[0]) << 8) | c.buf[1];
    // buf[2-3]: protocol id (must be 0x0000, not validated — be lenient)
    uint8_t  unitId = c.buf[6];
    uint8_t  fc     = c.buf[7];

    if (fc == 0x03 && pduLen >= 6) {
        uint16_t startAddr = (static_cast<uint16_t>(c.buf[8]) << 8)  | c.buf[9];
        uint16_t regCount  = (static_cast<uint16_t>(c.buf[10]) << 8) | c.buf[11];
        handleReadRegs(c.tcp, tid, unitId, startAddr, regCount);
    } else {
        sendException(c.tcp, tid, unitId, fc, 0x01); // ILLEGAL FUNCTION
    }

    // Consume processed frame from buffer
    c.buf.erase(c.buf.begin(), c.buf.begin() + total);
    return !c.buf.empty();
}

// ---------------------------------------------------------------------------
// FC 0x03 – Read Holding Registers
// ---------------------------------------------------------------------------

void ModbusServerClass::handleReadRegs(WiFiClient& client, uint16_t tid, uint8_t unitId,
                                        uint16_t startAddr, uint16_t regCount)
{
    if (regCount == 0 || regCount > 125) {
        sendException(client, tid, unitId, 0x03, 0x03); return;
    }
    if (startAddr < kBase || static_cast<uint32_t>(startAddr - kBase) + regCount > kTotalRegs) {
        sendException(client, tid, unitId, 0x03, 0x02); return;
    }

    auto inv = unitIdToInverter(unitId);
    if (!inv) {
        sendException(client, tid, unitId, 0x03, 0x02); return;
    }

    // Nameplate power rating is read exactly once by SunSpec clients, at
    // discovery time, and never refreshed afterwards. Refuse to answer
    // until it's known so a client can't discover us with a stale/zero
    // value baked in permanently. Once known it's known for good (nothing
    // resets DevInfo back to zero), so this only delays first discovery,
    // it doesn't affect normal reachable/unreachable cycling afterwards.
    if (inv->DevInfo()->getMaxPower() == 0) {
        sendException(client, tid, unitId, 0x03, 0x0B); return; // GATEWAY TARGET DEVICE FAILED TO RESPOND
    }

    uint16_t regs[kTotalRegs] = {};
    fillRegisters(regs, inv, unitId);

    uint16_t offset    = startAddr - kBase;
    uint8_t  byteCount = static_cast<uint8_t>(regCount * 2);
    uint16_t pduResp   = 3u + byteCount;

    uint8_t resp[6 + 3 + 125 * 2];
    uint8_t* p = resp;
    *p++ = static_cast<uint8_t>(tid >> 8); *p++ = static_cast<uint8_t>(tid & 0xFF);
    *p++ = 0x00; *p++ = 0x00;
    *p++ = static_cast<uint8_t>(pduResp >> 8); *p++ = static_cast<uint8_t>(pduResp & 0xFF);
    *p++ = unitId;
    *p++ = 0x03;
    *p++ = byteCount;
    for (uint16_t i = 0; i < regCount; i++) {
        uint16_t v = regs[offset + i];
        *p++ = static_cast<uint8_t>(v >> 8);
        *p++ = static_cast<uint8_t>(v & 0xFF);
    }
    size_t respLen = static_cast<size_t>(p - resp);
    if (client.write(resp, respLen) != respLen) {
        // Peer didn't drain a ~260-byte reply within the 1s cap set at
        // accept() - stalled or gone. Drop it instead of leaving a wedged
        // slot that would eat this timeout again on every future request.
        client.stop();
    }
}

// ---------------------------------------------------------------------------
// Exception response
// ---------------------------------------------------------------------------

void ModbusServerClass::sendException(WiFiClient& client, uint16_t tid, uint8_t unitId,
                                       uint8_t fc, uint8_t code)
{
    uint8_t resp[9] = {
        static_cast<uint8_t>(tid >> 8), static_cast<uint8_t>(tid & 0xFF),
        0x00, 0x00, 0x00, 0x03,
        unitId, static_cast<uint8_t>(fc | 0x80), code
    };
    if (client.write(resp, sizeof(resp)) != sizeof(resp)) {
        client.stop();
    }
}

// ---------------------------------------------------------------------------
// Unit ID → inverter (by serial)
// ---------------------------------------------------------------------------

std::shared_ptr<InverterAbstract> ModbusServerClass::unitIdToInverter(uint8_t unitId) const
{
    auto const& cfg = Configuration.get().ModbusServer;
    for (size_t i = 0; i < INV_MAX_COUNT; ++i) {
        if (cfg.Inverter[i].Serial == 0ULL) { break; }
        if (cfg.Inverter[i].UnitId == unitId) {
            return Hoymiles.getInverterBySerial(cfg.Inverter[i].Serial);
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Register map: SunS id, then each model (header + payload) back to back,
// then end marker. Rebuilt on every request - cheap (a few dozen floats),
// simpler than caching, and always reflects live inverter state.
// ---------------------------------------------------------------------------

void ModbusServerClass::fillRegisters(uint16_t* regs, std::shared_ptr<InverterAbstract> const& inv, uint8_t unitId) const
{
    uint16_t offset = 0;

    // SunS
    regs[offset + 0] = 0x5375; // 'Su'
    regs[offset + 1] = 0x6E53; // 'nS'
    offset += 2;

    regs[offset + 0] = SunSpecCommonModel1::id();
    regs[offset + 1] = SunSpecCommonModel1::length();
    SunSpecCommonModel1::fill(regs + offset + 2, inv, unitId);
    offset += 2 + SunSpecCommonModel1::length();

    regs[offset + 0] = SunSpecInverterModel101_103::id(inv);
    regs[offset + 1] = SunSpecInverterModel101_103::length();
    SunSpecInverterModel101_103::fill(regs + offset + 2, inv, unitId);
    offset += 2 + SunSpecInverterModel101_103::length();

    regs[offset + 0] = SunSpecNameplateModel120::id();
    regs[offset + 1] = SunSpecNameplateModel120::length();
    SunSpecNameplateModel120::fill(regs + offset + 2, inv, unitId);
    offset += 2 + SunSpecNameplateModel120::length();

    // End marker
    regs[offset + 0] = 0xFFFF;
    regs[offset + 1] = 0x0000;
}
