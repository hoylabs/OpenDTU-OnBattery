/* framehandler.cpp
 *
 * Arduino library to read from Victron devices using VE.Direct protocol.
 * Derived from Victron framehandler reference implementation.
 *
 * The MIT License
 *
 * Copyright (c) 2019 Victron Energy BV
 * Portions Copyright (C) 2020 Chris Terwilliger
 * https://github.com/cterwilliger/VeDirectFrameHandler
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * 2020.05.05 - 0.2 - initial release
 * 2020.06.21 - 0.2 - add MIT license, no code changes
 * 2020.08.20 - 0.3 - corrected #include reference
 * 2024.03.08 - 0.4 - adds the ability to send hex commands and disassemble hex messages
 * 2025.03.29 - 0.5 - add of transmit error counters
 */

#include <Arduino.h>
#include "VeDirectFrameHandler.h"
#include <LogHelper.h>

// The name of the record that contains the checksum.
static constexpr char checksumTagName[] = "CHECKSUM";

#undef TAG
static const char* TAG = "veDirect";
#define SUBTAG _logId

template<typename T>
VeDirectFrameHandler<T>::VeDirectFrameHandler() :
	_lastUpdate(0),
	_state(State::IDLE),
    _prevState(State::IDLE),
	_checksum(0),
	_textPointer(0),
	_hexSize(0),
	_name(""),
	_value(""),
	_debugIn(0),
	_lastByteMillis(0),
	_dataValid(false),
	_startUpPassed(false)
{
}

template<typename T>
void VeDirectFrameHandler<T>::init(char const* who, gpio_num_t rx, gpio_num_t tx, uint8_t hwSerialPort)
{
	_vedirectSerial = std::make_unique<HardwareSerial>(hwSerialPort);
	_vedirectSerial->setRxBufferSize(512); // increased from default (256) to 512 Byte to avoid overflow
	_vedirectSerial->end(); // make sure the UART will be re-initialized
	_vedirectSerial->begin(19200, SERIAL_8N1, rx, tx);
	_vedirectSerial->flush();
	_canSend = (tx != GPIO_NUM_NC);
	_debugIn = 0;
	_startUpPassed = false; // to obtain a complete dataset after a new start or restart
	_dataValid = false;     // data is not valid on start or restart
	snprintf(_logId, sizeof(_logId), "%s %d/%d", who, rx, tx);

	DTU_LOGI("init complete");
}

template<typename T>
void VeDirectFrameHandler<T>::dumpDebugBuffer() {
	DTU_LOGD("received serial input (%d Bytes)", _debugIn);
	LogHelper::dumpBytes(TAG, _logId, _debugBuffer.data(), _debugIn);
	_debugIn = 0;
}

template<typename T>
void VeDirectFrameHandler<T>::reset()
{
	_checksum = 0;
	_state = State::IDLE;
	_textData.clear();
}

template<typename T>
void VeDirectFrameHandler<T>::loop()
{
	// if the data is older than 10 seconds, it is no longer valid (millis() rollover safe)
	if (_dataValid && ((millis() - _lastUpdate) > (10 * 1000))) {
		_dataValid = false;     // data is now outdated
		_startUpPassed = false; // reset the start-up condition
	}

	while (_vedirectSerial->available()) {
		rxData(_vedirectSerial->read());
		_lastByteMillis = millis();
	}

	// there will never be a large gap between two bytes.
	// if such a large gap is observed, reset the state machine so it tries
	// to decode a new frame / hex messages once more data arrives.
	if ((State::IDLE != _state) && ((millis() - _lastByteMillis) > 500)) {
        setErrorCounter(veStruct::Error::TIMEOUT);
		DTU_LOGW("Resetting state machine (was %d) after timeout", static_cast<unsigned>(_state));
		dumpDebugBuffer();
		reset();
    }

    if ((millis() - _lastErrorCalcAndPrint) > 60*1000) {
        _lastErrorCalcAndPrint = millis();

        // calculate the average transmit errors per day
        _tmpFrame.transmitErrors_Day = static_cast<float>(_errorSumSinceStartup);
        float errorDays = esp_timer_get_time() / (24.0f*60*60*1000*1000); // 24h, use float to avoid int overflow
        if (errorDays > 1.0f) { _tmpFrame.transmitErrors_Day /= errorDays; }

        // no need to print the errors if log level is not at least INFO or if we do not have any
        if (DTU_LOG_IS_INFO && (_errorSumSinceStartup != 0)) {
            printErrorCounter();
        }
    }
}

static bool isValidChar(uint8_t inbyte)
{
	// Except the checksum and allowed control characters, everything should
	// be ASCII and non-ASCII values indicate data corruption.

	// Allowed control characters.
	if (inbyte == '\t' || inbyte == '\n' || inbyte == '\r') {
		return true;
	}
	// Valid ASCII non-control range.
	if (inbyte >= 32 && inbyte < 128) {
		return true;
	}
	// Invalid character indicating data corruption.
	return false;
}

/*
 *  rxData
 *  This function is called by loop() which passes a byte of serial data
 *  Based on Victron's example code. But using String and Map instead of pointer and arrays
 */
template<typename T>
void VeDirectFrameHandler<T>::rxData(uint8_t inbyte)
{
	if (DTU_LOG_IS_VERBOSE) {
		_debugBuffer[_debugIn] = inbyte;
		_debugIn = (_debugIn + 1) % _debugBuffer.size();
		if (0 == _debugIn) {
            setErrorCounter(veStruct::Error::DEBUG_BUFFER);
			DTU_LOGE("debug buffer overrun!");
		}
	}
	if (_state != State::CHECKSUM && !isValidChar(inbyte)) {
        setErrorCounter(veStruct::Error::NON_VALID_CHAR);
		DTU_LOGW("non-ASCII character 0x%02x, invalid frame", inbyte);
		reset();
		return;
	}

	if ( (inbyte == ':') && (_state != State::CHECKSUM) ) {

        // hex frame can interrupt text frame, but hex frame
        // never interrupt hex frame, if we reach this line with
        // _state == State::RECORD_HEX we had a transmit fault
        if (_state == State::RECORD_HEX) {
            setErrorCounter(veStruct::Error::NESTED_HEX);
        } else {
            // We just store the current state if we come from a text frame state
            _prevState = _state;
        }

		_state = State::RECORD_HEX;
		_hexSize = 0;
	}
	if (_state != State::RECORD_HEX) {
		_checksum += inbyte;
	}
	inbyte = toupper(inbyte);

	switch(_state) {
	case State::IDLE:
		/* wait for \n of the start of an record */
		switch(inbyte) {
		case '\n':
			_state = State::RECORD_BEGIN;
			break;
		case '\r': /* Skip */
		default:
			break;
		}
		break;
	case State::RECORD_BEGIN:
		_textPointer = _name;
		*_textPointer++ = inbyte;
		_state = State::RECORD_NAME;
		break;
	case State::RECORD_NAME:
		// The record name is being received, terminated by a \t
		switch(inbyte) {
		case '\t':
			// the Checksum record indicates a EOR
			if ( _textPointer < (_name + sizeof(_name)) ) {
				*_textPointer = 0; /* Zero terminate */
				if (strcmp(_name, checksumTagName) == 0) {
					_state = State::CHECKSUM;
					break;
				}
			}
			_textPointer = _value; /* Reset value pointer */
			_state = State::RECORD_VALUE;
			break;
		case '#': /* Ignore # from serial number*/
			break;
		default:
			// add byte to name, but do no overflow
			if ( _textPointer < (_name + sizeof(_name)) )
				*_textPointer++ = inbyte;
			break;
		}
		break;
	case State::RECORD_VALUE:
		// The record value is being received.  The \r indicates a new record.
		switch(inbyte) {
		case '\n':
			if ( _textPointer < (_value + sizeof(_value)) ) {
				*_textPointer = 0; // make zero ended
				_textData.push_back({_name, _value});
			}
			_state = State::RECORD_BEGIN;
			break;
		case '\r': /* Skip */
			break;
		default:
			// add byte to value, but do no overflow
			if ( _textPointer < (_value + sizeof(_value)) )
				*_textPointer++ = inbyte;
			break;
		}
		break;
	case State::CHECKSUM:
	{
		dumpDebugBuffer();
		if (_checksum == 0) {

			_frameContainsFieldV = false;
			for (auto const& event : _textData) {
				processTextData(event.first, event.second);
			}

			// A dataset can be fragmented across multiple frames,
			// so we give just frames containing the field-label "V" a timestamp to avoid
			// multiple timestamps on related data. We also take care to have the dataset complete
			// after a start or restart or fault before we set the data as valid.
			// Note: At startup, it may take up to 2 seconds for the first timestamp to be available
			if (_frameContainsFieldV)
			{
				if (_startUpPassed)
				{
					_lastUpdate = millis();
					_dataValid = true;
				}
				_startUpPassed = true;
			}
			frameValidEvent();
		}
		else {
            setErrorCounter(veStruct::Error::TEXT_CHECKSUM);
			DTU_LOGW("checksum 0x%02x != 0x00, invalid frame", _checksum);
		}
		reset();
		break;
	}
	case State::RECORD_HEX:
		_state = hexRxEvent(inbyte);
		break;
	}
}

/*
 * This function is called every time a new name/value is successfully parsed.  It writes the values to the temporary buffer.
 */
template<typename T>
void VeDirectFrameHandler<T>::processTextData(std::string const& name, std::string const& value) {
	DTU_LOGD("Text Data '%s' = '%s'", name.c_str(), value.c_str());

	if (processTextDataDerived(name, value)) { return; }

	if (name == "PID") {
		_tmpFrame.productID_PID = strtol(value.c_str(), nullptr, 0);
		return;
	}

	if (name == "SER") {
		strncpy(_tmpFrame.serialNr_SER, value.c_str(), sizeof(_tmpFrame.serialNr_SER));
		return;
	}

	if (name == "FW") {
		_tmpFrame.firmwareVer_FWE[0] = '\0';
		strncpy(_tmpFrame.firmwareVer_FW, value.c_str(), sizeof(_tmpFrame.firmwareVer_FW));
		return;
	}

	// some devices use "FWE" instead of "FW" for the firmware version.
	if (name == "FWE") {
		_tmpFrame.firmwareVer_FW[0] = '\0';
		strncpy(_tmpFrame.firmwareVer_FWE, value.c_str(), sizeof(_tmpFrame.firmwareVer_FWE));
		return;
	}

	if (name == "V") {
		_tmpFrame.batteryVoltage_V_mV = atol(value.c_str());
		_frameContainsFieldV = true; // frame contains the field-label "V"
		return;
	}

	if (name == "I") {
		_tmpFrame.batteryCurrent_I_mA = atol(value.c_str());
		return;
	}

	DTU_LOGI("Unknown text data '%s' (value '%s')", name.c_str(), value.c_str());
}

/*
 *  hexRxEvent
 *  This function records hex answers or async messages
 */
template<typename T>
typename VeDirectFrameHandler<T>::State VeDirectFrameHandler<T>::hexRxEvent(uint8_t inbyte)
{
	State ret = State::RECORD_HEX; // default - continue recording until end of frame

	switch (inbyte) {
	case '\n':
		// now we can analyse the hex message
		_hexBuffer[_hexSize] = '\0';
		VeDirectHexData data;
		if (disassembleHexData(data) && !hexDataHandler(data)) {
			DTU_LOGI("Unhandled Hex %s Response, addr: 0x%04X (%s), "
					"value: 0x%08X, flags: 0x%02X",
					data.getResponseAsString().data(),
					static_cast<unsigned>(data.addr),
					data.getRegisterAsString().data(),
					data.value, data.flags);
		}

		// restore previous state
		ret=_prevState;
		break;

	default:
		_hexBuffer[_hexSize++]=inbyte;

		if (_hexSize>=VE_MAX_HEX_LEN) { // oops -buffer overflow - something went wrong, we abort
            setErrorCounter(veStruct::Error::HEX_BUFFER);
			DTU_LOGE("hexRx buffer overflow - aborting read");
			_hexSize=0;
			ret = State::IDLE;
		}
	}

	return ret;
}

// Returns the millis() timestamp of the last successfully received dataset
// Note: Be aware of millis() rollover every 49 days
template<typename T>
uint32_t VeDirectFrameHandler<T>::getLastUpdate() const
{
	return _lastUpdate;
}

/*
 * Counts the transmit errors
 */
template<typename T>
void VeDirectFrameHandler<T>::setErrorCounter(veStruct::Error type)
{
    // Start-up can be in the middle of a VE.Direct transmit.
    // That errors must be ignored. We wait until the startup condition is passed
    if (_startUpPassed) {

        // increment the error counters
        _errorCounter.at(static_cast<size_t>(type))++;
        _errorSumSinceStartup++; // with 1 error per second we would overflow after 136 years

        // to avoid overflow, we divide all counters by 2 if one of them exceeds 50k
        for (size_t idx = 0; idx < _errorCounter.size(); ++idx) {
            if (_errorCounter.at(idx) > 50000) {
                for (auto& counter : _errorCounter) { counter /= 2; }
                break;
            }
        }
    }
}

/*
 * Prints the specific error counters every 60 seconds
 */
template<typename T>
void VeDirectFrameHandler<T>::printErrorCounter(void)
{
    DTU_LOGI("Average transmit errors per day: %0.1f 1/d", _tmpFrame.transmitErrors_Day);

    auto constexpr maxPerLine = 3; // maximum number of errors per line
    std::string sBuffer;
    for(size_t idx = 0; idx < _errorCounter.size(); ++idx) {
        sBuffer.append(_tmpFrame.getTransmitErrorAsString(static_cast<veStruct::Error>(idx)).data());
        sBuffer.append(": ");
        sBuffer.append(std::to_string(_errorCounter.at(static_cast<size_t>(idx))));

        if (((idx + 1) % maxPerLine == 0) || (idx == _errorCounter.size() - 1)) {
            DTU_LOGI("%s", sBuffer.c_str()); // print the buffer
            sBuffer.clear();
        } else {
            sBuffer.append(", "); // add a comma to separate the errors
        }
    }
}
