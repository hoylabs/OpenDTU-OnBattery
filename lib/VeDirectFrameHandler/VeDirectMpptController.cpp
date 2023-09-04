#include <Arduino.h>
#include "VeDirectMpptController.h"

#ifndef VICTRON_PIN_TX
#define VICTRON_PIN_TX 26      // HardwareSerial TX Pin
#endif

#ifndef VICTRON_PIN_RX
#define VICTRON_PIN_RX 25      // HardwareSerial RX Pin
#endif

VeDirectMpptController VeDirect;

VeDirectMpptController::VeDirectMpptController() 
{
}

void VeDirectMpptController::init(int8_t rx, int8_t tx, Print* msgOut, bool verboseLogging)
{
    _vedirectSerial = new HardwareSerial(1);
	_vedirectSerial->begin(19200, SERIAL_8N1, rx, tx);
    _vedirectSerial->flush();
	VeDirectFrameHandler::init(msgOut, verboseLogging);
	_msgOut->println("Finished init MPPTCOntroller");
}



void VeDirectMpptController::textRxEvent(char * name, char * value) {
    _msgOut->printf("Received Text Event %s: Value: %s\r\n", name, value );
	VeDirectFrameHandler::textRxEvent(name, value);
	if (strcmp(name, "LOAD") == 0) {
		if (strcmp(value, "ON") == 0)
			_tmpFrame.LOAD = true;
		else	
			_tmpFrame.LOAD = false;
	}
	else if (strcmp(name, "CS") == 0) {
		_tmpFrame.CS = atoi(value);
	}
	else if (strcmp(name, "ERR") == 0) {
		_tmpFrame.ERR = atoi(value);
	}
	else if (strcmp(name, "OR") == 0) {
		_tmpFrame.OR = strtol(value, nullptr, 0);
	}
	else if (strcmp(name, "MPPT") == 0) {
		_tmpFrame.MPPT = atoi(value);
	}
	else if (strcmp(name, "HSDS") == 0) {
		_tmpFrame.HSDS = atoi(value);
	}
	else if (strcmp(name, "VPV") == 0) {
		_tmpFrame.VPV = round(atof(value) / 10.0) / 100.0;
	}
	else if (strcmp(name, "PPV") == 0) {
		_tmpFrame.PPV = atoi(value);
	}
	else if (strcmp(name, "H19") == 0) {
		_tmpFrame.H19 = atof(value) / 100.0;
	}
	else if (strcmp(name, "H20") == 0) {
		_tmpFrame.H20 = atof(value) / 100.0;
	}
	else if (strcmp(name, "H21") == 0) {
		_tmpFrame.H21 = atoi(value);
	}
	else if (strcmp(name, "H22") == 0) {
		_tmpFrame.H22 = atof(value) / 100.0;
	}
	else if (strcmp(name, "H23") == 0) {
		_tmpFrame.H23 = atoi(value);
	}
}