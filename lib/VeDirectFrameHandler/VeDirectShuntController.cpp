#include <Arduino.h>
#include "VeDirectShuntController.h"

VeDirectShuntController::VeDirectShuntController()
{
}

void VeDirectShuntController::init(int8_t rx, int8_t tx, Print* msgOut, bool verboseLogging)
{
    _vedirectSerial = new HardwareSerial(2);
	_vedirectSerial->begin(19200, SERIAL_8N1, rx, tx);
    _vedirectSerial->flush();
	VeDirectFrameHandler::init(msgOut, verboseLogging);
	_msgOut->println("Finished init ShuntController");
}

void VeDirectShuntController::textRxEvent(char* name, char* value)
{
    VeDirectFrameHandler::textRxEvent(name, value, _tmpFrame);
	if (strcmp(name, "T") == 0) {
			//_tmpFrame.T = atoi(value);
	}
    else if (strcmp(name, "P") == 0) {
		_tmpFrame.P = atoi(value);
	}
    else if (strcmp(name, "CE") == 0) {
		//_tmpFrame.CE = atoi(value);
	}
    else if (strcmp(name, "SOC") == 0) {
		//_tmpFrame.SOC = atoi(value);
	}
    else if (strcmp(name, "TTG") == 0) {
		//_tmpFrame.TTG = atoi(value);
	}
    else if (strcmp(name, "H1") == 0) {
		//_tmpFrame.H1 = atoi(value);
	}
    else if (strcmp(name, "H2") == 0) {
		//_tmpFrame.H2 = atoi(value);
	}
    else if (strcmp(name, "H3") == 0) {
		//_tmpFrame.H3 = atoi(value);
	}
    else if (strcmp(name, "H4") == 0) {
		//_tmpFrame.H4 = atoi(value);
	}
    else if (strcmp(name, "H5") == 0) {
		//_tmpFrame.H5 = atoi(value);
	}
    else if (strcmp(name, "H6") == 0) {
		//_tmpFrame.H6 = atoi(value);
	}
    else if (strcmp(name, "H7") == 0) {
		//_tmpFrame.H7 = atoi(value);
	}
    else if (strcmp(name, "H8") == 0) {
		//_tmpFrame.H8 = atoi(value);
	}
    else if (strcmp(name, "H9") == 0) {
		//_tmpFrame.H9 = atoi(value);
	}
    else if (strcmp(name, "H10") == 0) {
		//_tmpFrame.H10 = atoi(value);
	}
    else if (strcmp(name, "H11") == 0) {
		//_tmpFrame.H11 = atoi(value);
	}
    else if (strcmp(name, "H12") == 0) {
		//_tmpFrame.H12 = atoi(value);
	}
    else if (strcmp(name, "H13") == 0) {
		//_tmpFrame.H13 = atoi(value);
	}
    else if (strcmp(name, "H14") == 0) {
		//_tmpFrame.H14 = atoi(value);
	}
    else if (strcmp(name, "H15") == 0) {
		//_tmpFrame.H15 = atoi(value);
	}
    else if (strcmp(name, "H16") == 0) {
		//_tmpFrame.H16 = atoi(value);
	}
    else if (strcmp(name, "H17") == 0) {
		//_tmpFrame.H17 = atoi(value);
	}
    else if (strcmp(name, "H18") == 0) {
		//_tmpFrame.H18 = atoi(value);
	}
    else if (strcmp(name, "H19") == 0) {
		//_tmpFrame.H19 = atoi(value);
	}
   
}
