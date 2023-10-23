#pragma once

#include <cstdint>

class SMA_HMClass {
public:
    void init();
    void loop();
    void event1();
    float getPowerTotal();
    float getPowerL1();
    float getPowerL2();
    float getPowerL3();
    uint32_t serial = 0;
private:
    uint32_t _lastUpdate = 0;
    float _powerMeterPower = 0.0;
    float _powerMeterL1 = 0.0;
    float _powerMeterL2 = 0.0;
    float _powerMeterL3 = 0.0;
    unsigned long previousMillis = 0;
};
extern SMA_HMClass SMA_HM;