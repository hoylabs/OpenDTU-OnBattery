/* VeDirectMpptController.cpp
 *
 * 
 * 2020.08.20 - 0.0 - ???
 * 2024.03.18 - 0.1 - add of: - temperature from "Smart Battery Sense" connected over VE.Smart network
 * 					  		  - temperature from internal MPPT sensor
 * 					  		  - "total DC input power" from MPPT's connected over VE.Smart network
 */

#include <Arduino.h>
#include "VeDirectMpptController.h"


// support for debugging, 0=without extended logging, 1=with extended logging
constexpr int MODUL_DEBUG = 0;


void VeDirectMpptController::init(int8_t rx, int8_t tx, Print* msgOut, bool verboseLogging, uint16_t hwSerialPort)
{
	VeDirectFrameHandler::init(rx, tx, msgOut, verboseLogging, hwSerialPort);
	_spData = std::make_shared<veMpptStruct>();
	if (_verboseLogging) { _msgOut->println("Finished init MPPTController"); }
}

bool VeDirectMpptController::isDataValid() const {
	return VeDirectFrameHandler::isDataValid(*_spData);
}

void VeDirectMpptController::textRxEvent(char* name, char* value)
{
	if (VeDirectFrameHandler::textRxEvent("MPPT", name, value, _tmpFrame)) {
		return;
	}

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
 *  frameValidEvent
 *  This function is called at the end of the received frame.
 */
void VeDirectMpptController::frameValidEvent() {
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

	_spData = std::make_shared<veMpptStruct>(_tmpFrame);
	_tmpFrame = {};
	_lastUpdate = millis();
}

/*
 * getCsAsString
 * This function returns the state of operations (CS) as readable text.
 */
frozen::string const& VeDirectMpptController::veMpptStruct::getCsAsString() const
{
	static constexpr frozen::map<uint8_t, frozen::string, 9> values = {
		{ 0,   "OFF" },
		{ 2,   "Fault" },
		{ 3,   "Bulk" },
		{ 4,   "Absorbtion" },
		{ 5,   "Float" },
		{ 7,   "Equalize (manual)" },
		{ 245, "Starting-up" },
		{ 247, "Auto equalize / Recondition" },
		{ 252, "External Control" }
	};

	return getAsString(values, CS);
}

/*
 * getMpptAsString
 * This function returns the state of MPPT (MPPT) as readable text.
 */
frozen::string const& VeDirectMpptController::veMpptStruct::getMpptAsString() const
{
	static constexpr frozen::map<uint8_t, frozen::string, 3> values = {
		{ 0, "OFF" },
		{ 1, "Voltage or current limited" },
		{ 2, "MPP Tracker active" }
	};

	return getAsString(values, MPPT);
}

/*
 * getErrAsString
 * This function returns error state (ERR) as readable text.
 */
frozen::string const& VeDirectMpptController::veMpptStruct::getErrAsString() const
{
	static constexpr frozen::map<uint8_t, frozen::string, 20> values = {
		{ 0,   "No error" },
		{ 2,   "Battery voltage too high" },
		{ 17,  "Charger temperature too high" },
		{ 18,  "Charger over current" },
		{ 19,  "Charger current reversed" },
		{ 20,  "Bulk time limit exceeded" },
		{ 21,  "Current sensor issue(sensor bias/sensor broken)" },
		{ 26,  "Terminals overheated" },
		{ 28,  "Converter issue (dual converter models only)" },
		{ 33,  "Input voltage too high (solar panel)" },
		{ 34,  "Input current too high (solar panel)" },
		{ 38,  "Input shutdown (due to excessive battery voltage)" },
		{ 39,  "Input shutdown (due to current flow during off mode)" },
		{ 40,  "Input" },
		{ 65,  "Lost communication with one of devices" },
		{ 67,  "Synchronisedcharging device configuration issue" },
		{ 68,  "BMS connection lost" },
		{ 116, "Factory calibration data lost" },
		{ 117, "Invalid/incompatible firmware" },
		{ 118, "User settings invalid" }
	};

	return getAsString(values, ERR);
}

/*
 * getOrAsString
 * This function returns the off reason (OR) as readable text.
 */
frozen::string const& VeDirectMpptController::veMpptStruct::getOrAsString() const
{
	static constexpr frozen::map<uint32_t, frozen::string, 10> values = {
		{ 0x00000000, "Not off" },
		{ 0x00000001, "No input power" },
		{ 0x00000002, "Switched off (power switch)" },
		{ 0x00000004, "Switched off (device moderegister)" },
		{ 0x00000008, "Remote input" },
		{ 0x00000010, "Protection active" },
		{ 0x00000020, "Paygo" },
		{ 0x00000040, "BMS" },
		{ 0x00000080, "Engine shutdown detection" },
		{ 0x00000100, "Analysing input voltage" }
	};

	return getAsString(values, OR);
}


/*
// loop()
// send hex commands to MPPT every 5 seconds
*/
void VeDirectMpptController::loop()
{
	VeDirectFrameHandler::loop();
	
	// Copy from the "VE.Direct Protocol" documentation
	// For firmware version v1.52 and below, when no VE.Direct queries are sent to the device, the
	// charger periodically sends human readable (TEXT) data to the serial port. For firmware
	// versions v1.53 and above, the charger always periodically sends TEXT data to the serial port.
	// --> We just use hex commandes for firmware >= 1.53 to keep text messages alive
	if (atoi(_spData->FW) >= 153 ) {
		if ((millis() - _lastPingTime) > 5000) {

			sendHexCommand(GET, 0x2027);	// MPPT total DC input power
			sendHexCommand(GET, 0xEDDB);	// MPPT internal temperature
			sendHexCommand(GET, 0xEDEC);	// "Smart Battery Sense" temperature
			sendHexCommand(GET, 0x200F);	// Network info
			_lastPingTime = millis();
		}
	}
}


/*
 * hexDataHandler()
 * analyse the content of VE.Direct hex messages
 * Handels the received hex data from the MPPT
 */
void VeDirectMpptController::hexDataHandler(VeDirectHexData const &data) {
    bool state = false;

	switch (data.rsp) {
	case R_GET:
	case R_ASYNC:    

		// check if MPPT internal temperature is available
		if(data.id == 0xEDDB) {
			_ExData.T = static_cast<int32_t>(data.value) * 10;	// conversion from unit [0.01°C] to unit [m°C]
			_ExData.Tts = millis();
			state = true;
			
			if constexpr(MODUL_DEBUG == 1)
				_msgOut->printf("[VE.Direct] debug: hexDataHandler(), MTTP Temperature: %.2f°C\r\n", _ExData.T/1000.0);
		}

		// check if temperature from "Smart Battery Sense" is available
		if(data.id == 0xEDEC) {
			_ExData.TSBS = static_cast<int32_t>(data.value) * 10 - 272150;  // conversion from unit [0.01K] to unit [m°C]
			_ExData.TSBSts = millis();
			state = true;
			
			if constexpr(MODUL_DEBUG == 1)
				_msgOut->printf("[VE.Direct] debug: hexDataHandler(), Battery Temperature: %.2f°C\r\n", _ExData.TSBS/1000.0);
		}

		// check if "Total DC power" is available
		if(data.id == 0x2027) {
			_ExData.TDCP = data.value * 10;		// conversion from unit [0.01W] to unit [mW]
			_ExData.TDCPts = millis();
			state = true;

			if constexpr(MODUL_DEBUG == 1)
				_msgOut->printf("[VE.Direct] debug: hexDataHandler(), Total Power: %.2fW\r\n", _ExData.TDCP/1000.0);
		}

		// check if connected MPPT is charge instance master
		// Hint: not used right now but maybe necessary for future extensions
		if(data.id == 0x200F) {
			_veMaster = ((data.value & 0x0F) == 0x02) ? true : false;
			state = true;

			if constexpr(MODUL_DEBUG == 1)
				_msgOut->printf("[VE.Direct] debug: hexDataHandler(), Networkmode: 0x%X\r\n", data.value);
		}
		break;
	default:
		break;	
	}

	if constexpr(MODUL_DEBUG == 1)
		_msgOut->printf("[VE.Direct] debug: hexDataHandler(): rsp: %i, id: 0x%04X, value: %i[0x%08X], text: %s\r\n",
				data.rsp, data.id, data.value, data.value, data.text);

	if (_verboseLogging && state) 
		_msgOut->printf("[VE.Direct] MPPT hex message: rsp: %i, id: 0x%04X, value: %i[0x%08X], text: %s\r\n",
			data.rsp, data.id, data.value, data.value, data.text);
}   