#pragma once

#include <Arduino.h>
#include "VeDirectFrameHandler.h"

class VeDirectShuntController : VeDirectFrameHandler {

public:

    VeDirectShuntController();

    void init(int8_t rx, int8_t tx, Print* msgOut, bool verboseLogging);

    
private:

    void textRxEvent(char * name, char * value);


};

