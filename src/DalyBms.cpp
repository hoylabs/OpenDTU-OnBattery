#include "DalyBms.h"
#include "Configuration.h"
#include "MessageOutput.h"
#include "PinMapping.h"
#include "Battery.h"

HardwareSerial DalyHwSerial(2);

bool DalyBms::init(bool verboseLogging)
{
    _verboseLogging = verboseLogging;

    const PinMapping_t& pin = PinMapping.get();
    MessageOutput.printf("[Daly BMS] rx = %d, tx = %d, wake = %d\r\n",
            pin.battery_rx, pin.battery_tx, pin.battery_txen);

    if (pin.battery_rx < 0 || pin.battery_tx < 0) {
        MessageOutput.println("[Daly BMS] Invalid RX/TX pin config");
        return false;
    }

    DalyHwSerial.begin(9600, SERIAL_8N1, pin.battery_rx, pin.battery_tx);
    DalyHwSerial.flush();

    memset(this->my_txBuffer, 0x00, XFER_BUFFER_LENGTH);
    clearGet();
    _stats->_state = "DalyOffline";
    _stats->_connectionState = false;

    return true;
}

void DalyBms::deinit()
{
    DalyHwSerial.end();
}

void DalyBms::loop()
{
    if (millis() > _lastRequest + _pollInterval * 1000 + 250) {

        if (millis() - previousTime >= DELAYTINME)
        {
            _stats->setManufacturer("Daly BMS");
            MessageOutput.printf("[Daly BMS] Request counter = %d\r\n", requestCounter );
            switch (requestCounter)
            {
            case 0:
                // requestCounter = sendCommand() ? (requestCounter + 1) : 0;
                requestCounter++;
                break;
            case 1:
                if (getPackMeasurements())
                {
                    _stats->_connectionState = true;
                    errorCounter = 0;
                    requestCounter++;
                    _stats->setLastUpdate(millis());
                }
                else
                {
                    requestCounter = 0;
                    if (errorCounter < ERRORCOUNTER)
                    {
                        errorCounter++;
                    }
                    else
                    {
                        errorCounter = 0;
                        //requestCallback();
                        clearGet();
                        _stats -> _connectionState = false;
                        _stats -> _state = "DalyOffline";
                    }
                }
                break;
            case 2:
                requestCounter = getMinMaxCellVoltage() ? (requestCounter + 1) : 0;
                break;
            case 3:
                requestCounter = getPackTemp() ? (requestCounter + 1) : 0;
                break;
            case 4:
                requestCounter = getDischargeChargeMosStatus() ? (requestCounter + 1) : 0;
                break;
            case 5:
                requestCounter = getStatusInfo() ? (requestCounter + 1) : 0;
                break;
            case 6:
                requestCounter = getCellVoltages() ? (requestCounter + 1) : 0;
                break;
            default:
                requestCounter = 1;
                break;
            }
            previousTime = millis();
        }
    }
}

bool DalyBms::getPackMeasurements() // 0x90
{
    if (!this->requestData(COMMAND::VOUT_IOUT_SOC, 1))
    {
        MessageOutput.println("<DALY-BMS DEBUG> Receive failed, V, I, & SOC values won't be modified!\n");
        clearGet();
        return false;
    }
    else
        // check if packCurrent in range
        if (((float)(((this->frameBuff[0][8] << 8) | this->frameBuff[0][9]) - 30000) / 10.0f) == -3000.f)
        {
            MessageOutput.println("<DALY-BMS DEBUG> Receive failed, pack Current not in range. values won't be modified!\n");
            return false;
        }
        else
            // check if SOC in range
            if (((float)((this->frameBuff[0][10] << 8) | this->frameBuff[0][11]) / 10.0f) > 100.f)
            {
                MessageOutput.println("<DALY-BMS DEBUG> Receive failed,SOC out of range. values won't be modified!\n");
               return false;
            }

    // Pull the relevent values out of the buffer
    _stats->_voltage = ((float)((this->frameBuff[0][4] << 8) | this->frameBuff[0][5]) / 10.0f);
    _stats->_current = ((float)(((this->frameBuff[0][8] << 8) | this->frameBuff[0][9]) - 30000) / 10.0f);
    _stats->_SoC = ((float)((this->frameBuff[0][10] << 8) | this->frameBuff[0][11]) / 10.0f);
    //MessageOutput.println("<DALY-BMS DEBUG> " + (String)get.packVoltage + "V, " + (String)get.packCurrent + "A, " + (String)get.packSOC + "SOC");
   return true;
}

bool DalyBms::getMinMaxCellVoltage() // 0x91
{
    if (!this->requestData(COMMAND::MIN_MAX_CELL_VOLTAGE, 1))
    {
        MessageOutput.printf("<DALY-BMS DEBUG> Receive failed, min/max cell values won't be modified!\n");
        return false;
    }

    _stats -> _maxCellmV = (float)((this->frameBuff[0][4] << 8) | this->frameBuff[0][5]);
    _stats -> _maxCellVNum = this->frameBuff[0][6];
    _stats -> _minCellmV = (float)((this->frameBuff[0][7] << 8) | this->frameBuff[0][8]);
    _stats -> _minCellVNum = this->frameBuff[0][9];
    _stats -> _cellDiff = ( _stats -> _maxCellmV -  _stats -> _minCellmV);

    return true;
}

bool DalyBms::getPackTemp() // 0x92
{
    if (!this->requestData(COMMAND::MIN_MAX_TEMPERATURE, 1))
    {
        MessageOutput.printf("<DALY-BMS DEBUG> Receive failed, Temp values won't be modified!\n");
        return false;
    }
    _stats -> _temperature = ((this->frameBuff[0][4] - 40) + (this->frameBuff[0][6] - 40)) / 2;

    return true;
}

bool DalyBms::getDischargeChargeMosStatus() // 0x93
{
    if (!this->requestData(COMMAND::DISCHARGE_CHARGE_MOS_STATUS, 1))
    {
         MessageOutput.printf("<DALY-BMS DEBUG> Receive failed, Charge / discharge mos Status won't be modified!\n");
        return false;
    }

    switch (this->frameBuff[0][4])
    {
    case 0:
        _stats -> _state = "DalyStationary";
        break;
    case 1:
        _stats -> _state = "DalyCharge";
        break;
    case 2:
        _stats -> _state = "DalyDischarge";
        break;
    }

    if (this->frameBuff[0][5]==0){
        _stats -> _chargeFetState = true;
    } else {
        _stats -> _chargeFetState = false;
    }
    if (this->frameBuff[0][6]==0){
        _stats -> _dischargeFetState = true;
    } else {
        _stats -> _dischargeFetState = true;
    }
    _stats -> _bmsHeartBeat = this->frameBuff[0][7];
    char msgbuff[16];
    float tmpAh = (((uint32_t)frameBuff[0][8] << 0x18) | ((uint32_t)frameBuff[0][9] << 0x10) | ((uint32_t)frameBuff[0][10] << 0x08) | (uint32_t)frameBuff[0][11]) * 0.001;
    dtostrf(tmpAh, 3, 1, msgbuff);
    _stats -> _resCapacityAh = atof(msgbuff);

    return true;
}

bool DalyBms::getStatusInfo() // 0x94
{
    if (!this->requestData(COMMAND::STATUS_INFO, 1))
    {
         MessageOutput.printf("<DALY-BMS DEBUG> Receive failed, Status info won't be modified!\n");
        return false;
    }

    _stats -> _numberOfCells = this->frameBuff[0][4];
    _stats -> _numOfTempSensors = this->frameBuff[0][5];
    _stats -> _chargeState = this->frameBuff[0][6];
    _stats -> _loadState = this->frameBuff[0][7];
    _stats -> _bmsCycles = ((uint16_t)this->frameBuff[0][9] << 0x08) | (uint16_t)this->frameBuff[0][10];

    return true;
}

bool DalyBms::getCellVoltages() // 0x95
{
    unsigned int cellNo = 0; // start with cell no. 1

    // Check to make sure we have a valid number of cells
    if (_stats -> _numberOfCells < MIN_NUMBER_CELLS && _stats -> _numberOfCells >= MAX_NUMBER_CELLS)
    {
        return false;
    }

    if (this->requestData(COMMAND::CELL_VOLTAGES, (unsigned int)ceil(_stats -> _numberOfCells / 3.0)))
    {
        for (size_t k = 0; k < (unsigned int)ceil(_stats -> _numberOfCells / 3.0); k++) // test for bug #67
        {
            for (size_t i = 0; i < 3; i++)
            {
                _stats -> _cellVmV[cellNo] = (this->frameBuff[k][5 + i + i] << 8) | this->frameBuff[k][6 + i + i];
                cellNo++;
                if (cellNo >= _stats -> _numberOfCells)
                    break;
            }
        }
        return true;
    }
    else
    {
        return false;
    }
}

bool DalyBms::setDischargeMOS(bool sw) // 0xD9 0x80 First Byte 0x01=ON 0x00=OFF
{
    if (sw)
    {
        MessageOutput.println("Attempting to switch discharge MOSFETs on");
        // Set the first byte of the data payload to 1, indicating that we want to switch on the MOSFET
        requestCounter = 0;
        this->my_txBuffer[4] = 0x01;
        this->sendCommand(COMMAND::DISCHRG_FET);
    }
    else
    {
        MessageOutput.println("Attempting to switch discharge MOSFETs off");
        requestCounter = 0;
        this->sendCommand(COMMAND::DISCHRG_FET);
    }
    if (!this->receiveBytes())
    {
         MessageOutput.printf("<DALY-BMS DEBUG> No response from BMS! Can't verify MOSFETs switched.\n");
        return false;
    }

    return true;
}

bool DalyBms::setChargeMOS(bool sw) // 0xDA 0x80 First Byte 0x01=ON 0x00=OFF
{
    if (sw == true)
    {
        MessageOutput.println("Attempting to switch charge MOSFETs on");
        // Set the first byte of the data payload to 1, indicating that we want to switch on the MOSFET
        requestCounter = 0;
        this->my_txBuffer[4] = 0x01;
        this->sendCommand(COMMAND::CHRG_FET);
    }
    else
    {
        MessageOutput.println("Attempting to switch charge MOSFETs off");
        requestCounter = 0;
        this->sendCommand(COMMAND::CHRG_FET);
    }

    if (!this->receiveBytes())
    {
        MessageOutput.println("<DALY-BMS DEBUG> No response from BMS! Can't verify MOSFETs switched.\n");
        return false;
    }

    return true;
}

bool DalyBms::setBmsReset() // 0x00 Reset the BMS
{
    requestCounter = 0;
    this->sendCommand(COMMAND::BMS_RESET);

    if (!this->receiveBytes())
    {
        MessageOutput.printf("<DALY-BMS DEBUG> Send failed, can't verify BMS was reset!\n");
        return false;
    }

    return true;
}

bool DalyBms::setSOC(float val) // 0x21 last two byte is SOC
{
    if (val >= 0 && val <= 100)
    {
        requestCounter = 0;

        MessageOutput.println("<DALY-BMS DEBUG> Attempting to read the SOC");
        // try read with 0x61
        this->sendCommand(COMMAND::READ_SOC);
        if (!this->receiveBytes())
        {
            // if 0x61 fails, write fake timestamp
            MessageOutput.println("<DALY-BMS DEBUG> Attempting to set the SOC with fake RTC data");

            this->my_txBuffer[5] = 0x17; // year
            this->my_txBuffer[6] = 0x01; // month
            this->my_txBuffer[7] = 0x01; // day
            this->my_txBuffer[8] = 0x01; // hour
            this->my_txBuffer[9] = 0x01; // minute
        }
        else
        {
            MessageOutput.println("<DALY-BMS DEBUG> Attempting to set the SOC with RTC data from BMS");
            for (size_t i = 5; i <= 9; i++)
            {
                this->my_txBuffer[i] = this->my_rxBuffer[i];
            }
        }
        uint16_t value = (val * 10);
        this->my_txBuffer[10] = (value & 0xFF00) >> 8;
        this->my_txBuffer[11] = (value & 0x00FF);
        this->sendCommand(COMMAND::SET_SOC);

        if (!this->receiveBytes())
        {
            MessageOutput.printf("<DALY-BMS DEBUG> No response from BMS! Can't verify SOC.\n");
            return false;
        }
        else
        {
            return true;
        }
    }
    return false;
}

bool DalyBms::getState() // Function to return the state of connection
{
    return _stats->_connectionState;
}

//----------------------------------------------------------------------
// Private Functions
//----------------------------------------------------------------------

bool DalyBms::requestData(COMMAND cmdID, unsigned int frameAmount) // new function to request global data
{
    // Clear out the buffers
    memset(this->my_rxFrameBuffer, 0x00, sizeof(this->my_rxFrameBuffer));
    memset(this->frameBuff, 0x00, sizeof(this->frameBuff));
    memset(this->my_txBuffer, 0x00, XFER_BUFFER_LENGTH);
    //--------------send part--------------------
    uint8_t txChecksum = 0x00;    // transmit checksum buffer
    unsigned int byteCounter = 0; // bytecounter for incomming data
    // prepare the frame with static data and command ID
    this->my_txBuffer[0] = START_BYTE;
    this->my_txBuffer[1] = HOST_ADRESS;
    this->my_txBuffer[2] = cmdID;
    this->my_txBuffer[3] = FRAME_LENGTH;

    // Calculate the checksum
    for (uint8_t i = 0; i <= 11; i++)
    {
        txChecksum += this->my_txBuffer[i];
    }
    // put it on the frame
    this->my_txBuffer[12] = txChecksum;

    // send the packet
    DalyHwSerial.write(this->my_txBuffer, XFER_BUFFER_LENGTH);
    // first wait for transmission end
    DalyHwSerial.flush();
    //-------------------------------------------

    //-----------Recive Part---------------------
    /*uint8_t rxByteNum = */ DalyHwSerial.readBytes(this->my_rxFrameBuffer, XFER_BUFFER_LENGTH * frameAmount);
    for (size_t i = 0; i < frameAmount; i++)
    {
        for (size_t j = 0; j < XFER_BUFFER_LENGTH; j++)
        {
            this->frameBuff[i][j] = this->my_rxFrameBuffer[byteCounter];
            byteCounter++;
        }

        uint8_t rxChecksum = 0x00;
        for (int k = 0; k < XFER_BUFFER_LENGTH - 1; k++)
        {
            rxChecksum += this->frameBuff[i][k];
        }
        char debugBuff[128];
        sprintf(debugBuff, "<UART>[Command: 0x%2X][CRC Rec: %2X][CRC Calc: %2X]", cmdID, rxChecksum, this->frameBuff[i][XFER_BUFFER_LENGTH - 1]);

        if (rxChecksum != this->frameBuff[i][XFER_BUFFER_LENGTH - 1])
        {
            MessageOutput.println("<UART> CRC FAIL");
            return false;
        }
        if (rxChecksum == 0)
        {
            MessageOutput.println("<UART> NO DATA");
            return false;
        }
        if (this->frameBuff[i][1] >= 0x20)
        {
            MessageOutput.println("<UART> BMS SLEEPING");
            return false;
        }
    }
    return true;
}

bool DalyBms::sendQueueAdd(COMMAND cmdID)
{

    for (size_t i = 0; i < sizeof commandQueue / sizeof commandQueue[0]; i++) // run over the queue array
    {
        if (commandQueue[i] == 0x100) // search the next free slot for command
        {
            commandQueue[i] = cmdID; // put in the command
            break;
        }
    }

    return true;
}

bool DalyBms::sendCommand(COMMAND cmdID)
{

    uint8_t checksum = 0;
    do // clear all incoming serial to avoid data collision
    {
        char t __attribute__((unused)) = DalyHwSerial.read(); // war auskommentiert, zum testen an

    } while (DalyHwSerial.read() > 0);

    // prepare the frame with static data and command ID
    this->my_txBuffer[0] = START_BYTE;
    this->my_txBuffer[1] = HOST_ADRESS;
    this->my_txBuffer[2] = cmdID;
    this->my_txBuffer[3] = FRAME_LENGTH;

    // Calculate the checksum
    for (uint8_t i = 0; i <= 11; i++)
    {
        checksum += this->my_txBuffer[i];
    }
    // put it on the frame
    this->my_txBuffer[12] = checksum;
    MessageOutput.println();
    MessageOutput.println(checksum, HEX);

    DalyHwSerial.write(this->my_txBuffer, XFER_BUFFER_LENGTH);
    // fix the sleep Bug
    // first wait for transmission end
    DalyHwSerial.flush();

    // after send clear the transmit buffer
    memset(this->my_txBuffer, 0x00, XFER_BUFFER_LENGTH);
    requestCounter = 0; // reset the request queue that we get actual data
    return true;
}

bool DalyBms::receiveBytes(void)
{
    // Clear out the input buffer
    memset(this->my_rxBuffer, 0x00, XFER_BUFFER_LENGTH);
    memset(this->frameBuff, 0x00, sizeof(this->frameBuff));

    // Read bytes from the specified serial interface
    uint8_t rxByteNum = DalyHwSerial.readBytes(this->my_rxBuffer, XFER_BUFFER_LENGTH);

    // Make sure we got the correct number of bytes
    if (rxByteNum != XFER_BUFFER_LENGTH)
    {
        MessageOutput.println("<DALY-BMS DEBUG> Error: Received the wrong number of bytes! Expected 13, got ");
        MessageOutput.println(rxByteNum, DEC);
        this->barfRXBuffer();
        return false;
    }

    if (!validateChecksum())
    {
        MessageOutput.println("<DALY-BMS DEBUG> Error: Checksum failed!");
        this->barfRXBuffer();

        return false;
    }

    return true;
}

bool DalyBms::validateChecksum()
{
    uint8_t checksum = 0x00;

    for (int i = 0; i < XFER_BUFFER_LENGTH - 1; i++)
    {
        checksum += this->my_rxBuffer[i];
    }
    MessageOutput.println("<DALY-BMS DEBUG> CRC: Calc.: " + (String)checksum + " Rec.: " + (String)this->my_rxBuffer[XFER_BUFFER_LENGTH - 1]);
    // Compare the calculated checksum to the real checksum (the last received byte)
    return (checksum == this->my_rxBuffer[XFER_BUFFER_LENGTH - 1]);
}

void DalyBms::barfRXBuffer(void)
{
    MessageOutput.println("<DALY-BMS DEBUG> RX Buffer: [");
    for (int i = 0; i < XFER_BUFFER_LENGTH; i++)
    {
        MessageOutput.println(",0x" + (String)this->my_rxBuffer[i]);
    }
    MessageOutput.println("]");
}

void DalyBms::clearGet(void)
{
    _stats -> _connectionState = false;
    _stats -> _state = "DalyOffline"; // charge/discharge status (0 stationary ,1 charge ,2 discharge)
}
