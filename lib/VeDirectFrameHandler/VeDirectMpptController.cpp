#include <Arduino.h>
#include "VeDirectMpptController.h"

void VeDirectMpptController::init(int8_t rx, int8_t tx, Print* msgOut, bool verboseLogging)
{
	VeDirectFrameHandler::init(rx, tx, msgOut, verboseLogging, 1);
	if (_verboseLogging) { _msgOut->println("Finished init MPPTController"); }
}

bool VeDirectMpptController::isDataValid() const {
	return VeDirectFrameHandler::isDataValid(veFrame);
}

void VeDirectMpptController::textRxEvent(char * name, char * value) {
	if (_verboseLogging) { _msgOut->printf("[Victron MPPT] Received Text Event %s: Value: %s\r\n", name, value ); }
	VeDirectFrameHandler::textRxEvent(name, value, _tmpFrame);
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

/*
 *  frameEndEvent
 *  This function is called at the end of the received frame.  If the checksum is valid, the temp buffer is read line by line.
 *  If the name exists in the public buffer, the new value is copied to the public buffer.	If not, a new name/value entry
 *  is created in the public buffer.
 */
void VeDirectMpptController::frameEndEvent(bool valid) {
	if (valid) {
		_tmpFrame.P = _tmpFrame.V * _tmpFrame.I;

		_tmpFrame.IPV = 0;
		if (_tmpFrame.VPV > 0) {
			_tmpFrame.IPV = _tmpFrame.PPV / _tmpFrame.VPV;
		}

		_tmpFrame.E = 0;
		if ( _tmpFrame.PPV > 0) {
			_efficiency.addNumber(static_cast<double>(_tmpFrame.P * 100) / _tmpFrame.PPV);
			_tmpFrame.E = _efficiency.getAverage();
		}

		veFrame = _tmpFrame;
		_tmpFrame = {};
		_lastUpdate = millis();
	}
}

/*
 * getCsAsString
 * This function returns the state of operations (CS) as readable text.
 */
String VeDirectMpptController::veMpptStruct::getCsAsString() const
{
	String strCS ="";

	switch(CS) {
		case 0:
			strCS =  "OFF";
			break;
		case 2:
			strCS =  "Fault";
			break;
		case 3:
			strCS =  "Bulk";
			break;
		case 4:
			strCS =  "Absorbtion";
			break;
		case 5:
			strCS =  "Float";
			break;
		case 7:
			strCS =  "Equalize (manual)";
			break;
		case 245:
			strCS =  "Starting-up";
			break;
		case 247:
			strCS =  "Auto equalize / Recondition";
			break;
		case 252:
			strCS =  "External Control";
			break;
		default:
			strCS = CS;
	}
	return strCS;
}

/*
 * getMpptAsString
 * This function returns the state of MPPT (MPPT) as readable text.
 */
String VeDirectMpptController::veMpptStruct::getMpptAsString() const
{
	String strMPPT ="";

	switch(MPPT) {
		case 0:
			strMPPT =  "OFF";
			break;
		case 1:
			strMPPT =  "Voltage or current limited";
			break;
		case 2:
			strMPPT =  "MPP Tracker active";
			break;
		default:
			strMPPT = MPPT;
	}
	return strMPPT;
}

/*
 * getErrAsString
 * This function returns error state (ERR) as readable text.
 */
String VeDirectMpptController::veMpptStruct::getErrAsString() const
{
	String strERR ="";

	switch(ERR) {
		case 0:
			strERR =  "No error";
			break;
		case 2:
			strERR =  "Battery voltage too high";
			break;
		case 17:
			strERR =  "Charger temperature too high";
			break;
		case 18:
			strERR =  "Charger over current";
			break;
		case 19:
			strERR =  "Charger current reversed";
			break;
		case 20:
			strERR =  "Bulk time limit exceeded";
			break;
		case 21:
			strERR =  "Current sensor issue(sensor bias/sensor broken)";
			break;
		case 26:
			strERR =  "Terminals overheated";
			break;
		case 28:
			strERR =  "Converter issue (dual converter models only)";
			break;
		case 33:
			strERR =  "Input voltage too high (solar panel)";
			break;
		case 34:
			strERR =  "Input current too high (solar panel)";
			break;
		case 38:
			strERR =  "Input shutdown (due to excessive battery voltage)";
			break;
		case 39:
			strERR =  "Input shutdown (due to current flow during off mode)";
			break;
		case 40:
			strERR =  "Input";
			break;
		case 65:
			strERR =  "Lost communication with one of devices";
			break;
		case 67:
			strERR =  "Synchronisedcharging device configuration issue";
			break;
		case 68:
			strERR =  "BMS connection lost";
			break;
		case 116:
			strERR =  "Factory calibration data lost";
			break;
		case 117:
			strERR =  "Invalid/incompatible firmware";
			break;
		case 118:
			strERR =  "User settings invalid";
			break;
		default:
			strERR = ERR;
	}
	return strERR;
}

/*
 * getOrAsString
 * This function returns the off reason (OR) as readable text.
 */
String VeDirectMpptController::veMpptStruct::getOrAsString() const
{
	String strOR ="";

	switch(OR) {
		case 0x00000000:
			strOR =  "Not off";
			break;
		case 0x00000001:
			strOR =  "No input power";
			break;
		case 0x00000002:
			strOR =  "Switched off (power switch)";
			break;
		case 0x00000004:
			strOR =  "Switched off (device moderegister)";
			break;
		case 0x00000008:
			strOR =  "Remote input";
			break;
		case 0x00000010:
			strOR =  "Protection active";
			break;
		case 0x00000020:
			strOR =  "Paygo";
			break;
		case 0x00000040:
			strOR =  "BMS";
			break;
		case 0x00000080:
			strOR =  "Engine shutdown detection";
			break;
		case 0x00000100:
			strOR =  "Analysing input voltage";
			break;
		default:
			strOR = OR;
	}
	return strOR;
}
