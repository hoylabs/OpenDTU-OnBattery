/*
DALY2MQTT Project
https://github.com/softwarecrash/DALY2MQTT
*/
#include "SoftwareSerial.h"
#include "Battery.h"
#ifndef DALY_BMS_UART_H
#define DALY_BMS_UART_H

#define XFER_BUFFER_LENGTH 13
#define MIN_NUMBER_CELLS 1
#define MAX_NUMBER_CELLS 48
#define MIN_NUMBER_TEMP_SENSORS 1
#define MAX_NUMBER_TEMP_SENSORS 16

#define START_BYTE 0xA5; // Start byte
#define HOST_ADRESS 0x40; // Host address
#define FRAME_LENGTH 0x08; // Length
#define ERRORCOUNTER 10 //number of try befor clear data

//time in ms for delay the bms requests, to fast brings connection error

#define DELAYTINME 100

class DalyBms  : public BatteryProvider {
public:
    bool init(bool verboseLogging) final;
    void deinit() final;
    void loop() final;
    std::shared_ptr<BatteryStats> getStats() const final { return _stats; }

    unsigned long previousTime = 0;
    byte requestCounter = 0;
    String failCodeArr;

    enum COMMAND
    {
        CELL_THRESHOLDS = 0x59,
        PACK_THRESHOLDS = 0x5A,
        VOUT_IOUT_SOC = 0x90,
        MIN_MAX_CELL_VOLTAGE = 0x91,
        MIN_MAX_TEMPERATURE = 0x92,
        DISCHARGE_CHARGE_MOS_STATUS = 0x93,
        STATUS_INFO = 0x94,
        CELL_VOLTAGES = 0x95,
        CELL_TEMPERATURE = 0x96,
        CELL_BALANCE_STATE = 0x97,
        FAILURE_CODES = 0x98,
        DISCHRG_FET = 0xD9,
        CHRG_FET = 0xDA,
        BMS_RESET = 0x00,
        READ_SOC = 0x61, //read the time and soc
        SET_SOC = 0x21, //set the time and soc
        //END = 0xD8,
        //after request the pc soft hangs a 0xD8 as last request, its empty, dont know what it means?
    };


    /**
     * @brief Gets Voltage, Current, and SOC measurements from the BMS
     * @return True on successful aquisition, false otherwise
     */
    bool getPackMeasurements();

    /**
     * @brief Gets the pack temperature from the min and max of all the available temperature sensors
     * @details Populates tempMax, tempMax, and tempAverage in the "get" struct
     * @return True on successful aquisition, false otherwise
     */
    bool getPackTemp();

    /**
     * @brief Returns the highest and lowest individual cell voltage, and which cell is highest/lowest
     * @details Voltages are returned as floats with milliVolt precision (3 decimal places)
     * @return True on successful aquisition, false otherwise
     */
    bool getMinMaxCellVoltage();

    /**
     * @brief Get the general Status Info
     *
     */
    bool getStatusInfo();

    /**
     * @brief Get Cell Voltages
     *
     */
    bool getCellVoltages();

    /**
     * @brief
     * set the Discharging MOS State
     */
    bool setDischargeMOS(bool sw);

    /**
     * @brief set the Charging MOS State
     *
     */
    bool setChargeMOS(bool sw);

    /**
     * @brief set the SOC
     *
     */
    bool setSOC(float sw);

    /**
     * @brief Read the charge and discharge MOS States
     *
     */
    bool getDischargeChargeMosStatus();

    /**
     * @brief Reseting The BMS
     * @details Reseting the BMS and let it restart
     */
    bool setBmsReset();

    /**
     * @brief return the state of connection to the BMS
     * @details returns the following value for different connection state
     * -3 - could not open serial port
     * -2 - no data recived or wrong crc, check connection
     * -1 - working and collecting data, please wait
     *  0 - All data recived with correct crc, idleing
     *
     * now changed to bool, only true if data avaible, false when no connection
     */
    bool getState();

private:
    bool _verboseLogging = true;
    std::shared_ptr<DalyBatteryStats> _stats =
        std::make_shared<DalyBatteryStats>();

    bool getStaticData = false;
    unsigned int errorCounter = 0;
    unsigned int requestCount = 0;
    uint32_t _lastRequest = 0;
    uint8_t _pollInterval = 5;
    unsigned int commandQueue[5] = {0x100, 0x100, 0x100, 0x100, 0x100};
    /**
     * @brief send the command id, and return true if data complete read or false by crc error
     * @details calculates the checksum and sends the command over the specified serial connection
     */
    bool requestData(COMMAND cmdID, unsigned int frameAmount);

    /**
     * @brief Sends a complete packet with the specified command
     * @details calculates the checksum and sends the command over the specified serial connection
     */
    bool sendCommand(COMMAND cmdID);

    /**
     * @brief command queue
     */
    bool sendQueueAdd(COMMAND cmdID);

    /**
     * @brief Send the command ID to the BMS
     * @details
     * @return True on success, false on failure
     */
    bool receiveBytes(void);

    /**
     * @brief Validates the checksum in the RX Buffer
     * @return true if checksum matches, false otherwise
     */
    bool validateChecksum();

    /**
     * @brief Prints out the contense of the RX buffer
     * @details Useful for debugging
     */
    void barfRXBuffer();

    /**
     * @brief Clear all data from the Get struct
     * @details when wrong or missing data comes in it need sto be cleared
     */
    void clearGet();

    /**
     * @brief Buffer used to transmit data to the BMS
     * @details Populated primarily in the "Init()" function, see the readme for more info
     */
    uint8_t my_txBuffer[XFER_BUFFER_LENGTH];

    /**
     * @brief Buffer filled with data from the BMS
     */
    uint8_t my_rxBuffer[XFER_BUFFER_LENGTH];


uint8_t my_rxFrameBuffer[XFER_BUFFER_LENGTH*12];
uint8_t frameBuff[12][XFER_BUFFER_LENGTH];
unsigned int frameCount;
};

#endif // DALY_BMS_UART_H
