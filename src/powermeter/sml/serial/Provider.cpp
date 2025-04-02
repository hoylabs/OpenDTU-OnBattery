// SPDX-License-Identifier: GPL-2.0-or-later
#include <powermeter/sml/serial/Provider.h>
#include <PinMapping.h>
#include <MessageOutput.h>

namespace PowerMeters::Sml::Serial {

bool Provider::init()
{
    const PinMapping_t& pin = PinMapping.get();

    MessageOutput.printf("[PowerMeters::Sml::Serial] rx = %d\r\n", pin.powermeter_rx);

    if (pin.powermeter_rx <= GPIO_NUM_NC) {
        MessageOutput.println("[PowerMeters::Sml::Serial] invalid pin config "
                "for serial SML power meter (RX pin must be defined)");
        return false;
    }

    pinMode(pin.powermeter_rx, INPUT);
    _upSmlSerial = std::make_unique<SoftwareSerial>();
    _upSmlSerial->begin(_baud, SWSERIAL_8N1, pin.powermeter_rx, -1/*tx pin*/,
            false/*invert*/, _bufCapacity, _isrCapacity);
    _upSmlSerial->enableRx(true);
    _upSmlSerial->enableTx(false);
    _upSmlSerial->flush();

    return true;
}

void Provider::loop()
{
    if (_taskHandle != nullptr) { return; }

    std::unique_lock<std::mutex> lock(_pollingMutex);
    _stopPolling = false;
    lock.unlock();

    uint32_t constexpr stackSize = 3072;
    xTaskCreate(Provider::pollingLoopHelper, "PM:SML",
            stackSize, this, 1/*prio*/, &_taskHandle);
}

Provider::~Provider()
{
    _taskDone = false;

    std::unique_lock<std::mutex> lock(_pollingMutex);
    _stopPolling = true;
    lock.unlock();

    if (_taskHandle != nullptr) {
        while (!_taskDone) { delay(10); }
        _taskHandle = nullptr;
    }

    if (_upSmlSerial) {
        _upSmlSerial->end();
        _upSmlSerial = nullptr;
    }
}

void Provider::pollingLoopHelper(void* context)
{
    auto pInstance = static_cast<Provider*>(context);
    pInstance->pollingLoop();
    pInstance->_taskDone = true;
    vTaskDelete(nullptr);
}

void Provider::pollingLoop()
{
    int lastAvailable = 0;
    uint32_t gapStartMillis = 0;
    std::unique_lock<std::mutex> lock(_pollingMutex);

    while (!_stopPolling) {
        lock.unlock();

        // calling available() will decode bytes into the receive buffer and
        // hence free data from the ISR buffer, so we need to call this rather
        // frequenly.
        int nowAvailable = _upSmlSerial->available();

        if (nowAvailable <= 0) {
            // sleep, but at most until the software serial ISR
            // buffer is potentially half full with transitions.
            uint32_t constexpr delayMs = _isrCapacity * 1000 / _baud / 2;

            delay(delayMs); // this yields so other tasks are scheduled

            lock.lock();
            continue;
        }

        // sleep more if new data arrived in the meantime. process data only
        // once a SML datagram seems to be complete (no new data arrived while
        // we slept). this seems to be important as using read() while more
        // data arrives causes trouble (we are missing bytes).
        if (nowAvailable > lastAvailable) {
            lastAvailable = nowAvailable;
            delay(10);
            gapStartMillis = millis();
            lock.lock();
            continue;
        }

        if ((millis() - gapStartMillis) < _datagramGapMillis) {
            delay(10);
            lock.lock();
            continue;
        }

        while (_upSmlSerial->available() > 0) {
            processSmlByte(_upSmlSerial->read());
        }

        lastAvailable = 0;

        ::PowerMeters::Sml::Provider::reset();

        lock.lock();
    }
}

} // namespace PowerMeters::Sml::Serial
