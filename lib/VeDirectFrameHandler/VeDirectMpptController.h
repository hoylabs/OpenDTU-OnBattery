#pragma once

#include <Arduino.h>
#include "VeDirectFrameHandler.h"

class VeDirectMpptController : public VeDirectFrameHandler {

public:

    VeDirectMpptController();

    void init(int8_t rx, int8_t tx, Print* msgOut, bool verboseLogging);
    String getMpptAsString(uint8_t mppt);    // state of mppt as string
    String getCsAsString(uint8_t cs);        // current state as string
    bool isDataValid();                          // return true if data valid and not outdated
 
    struct veMpptStruct : veStruct {
        uint8_t  MPPT;                  // state of MPP tracker
        int32_t PPV;                    // panel power in W
        double VPV;                     // panel voltage in V
        double IPV;                     // panel current in A (calculated)
        bool LOAD;                      // virtual load output state (on if battery voltage reaches upper limit, off if battery reaches lower limit)
        uint8_t  CS;                    // current state of operation e. g. OFF or Bulk
        uint8_t ERR;                    // error code
        uint32_t OR;                    // off reason
        uint32_t HSDS;                  // day sequence number 1...365
        double H19;                     // yield total kWh
        double H20;                     // yield today kWh
        int32_t H21;                    // maximum power today W
        double H22;                     // yield yesterday kWh
        int32_t H23;                    // maximum power yesterday W
    };

    veMpptStruct veFrame{}; 

private:

    virtual void textRxEvent(char * name, char * value);
    void frameEndEvent(bool) override;                 // copy temp struct to public struct
    veMpptStruct _tmpFrame{};                        // private struct for received name and value pairs


};

extern VeDirectMpptController VeDirectMppt;
