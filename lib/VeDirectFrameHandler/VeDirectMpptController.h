#pragma once

#include <Arduino.h>
#include "VeDirectFrameHandler.h"

/*typedef struct {
    uint16_t PID;                   // product id
    char SER[VE_MAX_VALUE_LEN];     // serial number
    char FW[VE_MAX_VALUE_LEN];      // firmware release number
    bool LOAD;                      // virtual load output state (on if battery voltage reaches upper limit, off if battery reaches lower limit)
    uint8_t  CS;                    // current state of operation e. g. OFF or Bulk
    uint8_t ERR;                    // error code
    uint32_t OR;                    // off reason
    uint8_t  MPPT;                  // state of MPP tracker
    uint32_t HSDS;                  // day sequence number 1...365
    int32_t P;                      // battery output power in W (calculated)
    double V;                       // battery voltage in V
    double I;                       // battery current in A
    double E;                       // efficiency in percent (calculated, moving average)
    int32_t PPV;                    // panel power in W
    double VPV;                     // panel voltage in V
    double IPV;                     // panel current in A (calculated)
    double H19;                     // yield total kWh
    double H20;                     // yield today kWh
    int32_t H21;                   // maximum power today W
    double H22;                     // yield yesterday kWh
    int32_t H23;                   // maximum power yesterday W
} veMpptStruct;*/

class VeDirectMpptController : public VeDirectFrameHandler {

public:

    VeDirectMpptController();

    void init(int8_t rx, int8_t tx, Print* msgOut, bool verboseLogging);

    //veStruct veFrame{}; 

private:

    void textRxEvent(char * name, char * value);

};

extern VeDirectMpptController VeDirect;
