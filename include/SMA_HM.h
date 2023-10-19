#pragma once

#include <cstdint>

class SMA_HMClass {
public:
    void init();
    void loop();
    float getPowerTotal();
    uint32_t serial = 0;
private:
    uint32_t _lastUpdate = 0;
    float _powerMeterPower = 0.0;
    unsigned long previousMillis = 0;
    void event1();
};
extern SMA_HMClass SMA_HM;