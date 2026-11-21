// SPDX-License-Identifier: GPL-2.0-or-later
#include <powermeter/modbus/tcp/Provider.h>
#include <LogHelper.h>
#include <WiFi.h>
#include <cstring> // for memcpy

#undef TAG
static const char* TAG = "powerMeter";
static const char* SUBTAG = "ModbusTCP";

namespace PowerMeters::Modbus::Tcp {

Provider::Provider(PowerMeterModbusTcpConfig const& cfg)
    : _cfg(cfg)
{
}

bool Provider::init()
{
    if (WiFi.status() != WL_CONNECTED) {
        DTU_LOGE("WiFi not connected, cannot initialize Modbus TCP power meter");
        return false;
    }

    IPAddress ipAddr(_cfg.IpAddress[0], _cfg.IpAddress[1], _cfg.IpAddress[2], _cfg.IpAddress[3]);
    DTU_LOGI("Initializing Modbus TCP power meter: IP = %s, Port = %d, Device ID = %d",
             ipAddr.toString().c_str(), _cfg.Port, _cfg.DeviceId);

    _upWiFiClient = std::make_unique<WiFiClient>();

    return true;
}

Provider::~Provider()
{
    _taskDone = false;

    std::unique_lock<std::mutex> lock(_pollingMutex);
    _stopPolling = true;
    lock.unlock();

    _cv.notify_all();

    if (_taskHandle != nullptr) {
        while (!_taskDone) { delay(10); }
        _taskHandle = nullptr;
    }

    if (_upWiFiClient) {
        _upWiFiClient->stop();
        _upWiFiClient = nullptr;
    }
}

void Provider::loop()
{
    if (_taskHandle != nullptr) { return; }

    std::unique_lock<std::mutex> lock(_pollingMutex);
    _stopPolling = false;
    lock.unlock();

    uint32_t constexpr stackSize = 4096;
    xTaskCreate(Provider::pollingLoopHelper, "PM:ModbusTCP",
            stackSize, this, 1/*prio*/, &_taskHandle);
}

bool Provider::isDataValid() const
{
    uint32_t age = millis() - getLastUpdate();
    return getLastUpdate() > 0 && (age < (3 * _cfg.PollingInterval * 1000));
}

void Provider::pollingLoopHelper(void* context)
{
    auto pInstance = static_cast<Provider*>(context);
    pInstance->pollingLoop();
    pInstance->_taskDone = true;
    vTaskDelete(nullptr);
}

bool Provider::connectToDevice()
{
    if (_upWiFiClient->connected()) {
        return true;
    }

    IPAddress ipAddr(_cfg.IpAddress[0], _cfg.IpAddress[1], _cfg.IpAddress[2], _cfg.IpAddress[3]);

    DTU_LOGD("Connecting to Modbus TCP device at %s:%d", ipAddr.toString().c_str(), _cfg.Port);

    if (!_upWiFiClient->connect(ipAddr, _cfg.Port)) {
        DTU_LOGE("Failed to connect to Modbus TCP device");
        return false;
    }

    DTU_LOGD("Connected to Modbus TCP device");
    return true;
}

bool Provider::sendModbusRequest(uint8_t deviceId, uint8_t functionCode, uint16_t startRegister, uint16_t numRegisters)
{
    // Modbus TCP Application Data Unit (ADU) format:
    // [Transaction ID: 2 bytes][Protocol ID: 2 bytes][Length: 2 bytes][Unit ID: 1 byte][Function Code: 1 byte][Data: N bytes]

    uint8_t request[12];

    // MBAP Header
    request[0] = (_transactionId >> 8) & 0xFF;  // Transaction ID high byte
    request[1] = _transactionId & 0xFF;         // Transaction ID low byte
    request[2] = 0x00;                          // Protocol ID high byte (0 for Modbus)
    request[3] = 0x00;                          // Protocol ID low byte
    request[4] = 0x00;                          // Length high byte (6 bytes following)
    request[5] = 0x06;                          // Length low byte

    // PDU
    request[6] = deviceId;                      // Unit ID
    request[7] = functionCode;                  // Function code (0x03 = Read Holding Registers, 0x04 = Read Input Registers)
    request[8] = (startRegister >> 8) & 0xFF;  // Starting address high byte
    request[9] = startRegister & 0xFF;          // Starting address low byte
    request[10] = (numRegisters >> 8) & 0xFF;  // Quantity high byte
    request[11] = numRegisters & 0xFF;          // Quantity low byte

    _transactionId++;

    size_t bytesWritten = _upWiFiClient->write(request, sizeof(request));
    if (bytesWritten != sizeof(request)) {
        DTU_LOGE("Failed to send complete Modbus request, sent %d of %d bytes", bytesWritten, sizeof(request));
        return false;
    }

    DTU_LOGD("Sent Modbus request: Device ID=%d, Function Code=0x%02X, Start Reg=%d, Num Regs=%d", deviceId, functionCode, startRegister, numRegisters);
    return true;
}

bool Provider::receiveModbusResponse(uint16_t* values, uint16_t numRegisters)
{
    // Wait for response with timeout
    uint32_t startTime = millis();
    const uint32_t timeout = 5000; // 5 second timeout

    while (_upWiFiClient->available() < 9 && (millis() - startTime) < timeout) {
        delay(10);
    }

    if (_upWiFiClient->available() < 9) {
        DTU_LOGE("Modbus response timeout");
        return false;
    }

    // MBAP header + PDU + data
    size_t expectedLength = 9 + (numRegisters * 2);
    std::vector<uint8_t> buffer(expectedLength);

    // Wait for complete response
    startTime = millis();
    while (_upWiFiClient->available() < expectedLength && (millis() - startTime) < timeout) {
        delay(10);
    }

    if (_upWiFiClient->available() < expectedLength) {
        DTU_LOGE("Incomplete Modbus response, expected %d bytes, got %d", expectedLength, _upWiFiClient->available());
        return false;
    }

    size_t bytesRead = _upWiFiClient->readBytes(buffer.data(), expectedLength);
    if (bytesRead != expectedLength) {
        DTU_LOGE("Failed to read complete Modbus response");
        return false;
    }

    // Validate response
    uint16_t responseTransactionId = (buffer[0] << 8) | buffer[1];
    uint16_t expectedTransactionId = _transactionId - 1;

    if (responseTransactionId != expectedTransactionId) {
        DTU_LOGE("Transaction ID mismatch: expected %d, got %d", expectedTransactionId, responseTransactionId);
        return false;
    }

    uint8_t functionCode = buffer[7];
    if (functionCode != 0x04) { // Function code for input registers
        DTU_LOGE("Unexpected function code in response: 0x%02X", functionCode);
        return false;
    }

    uint8_t byteCount = buffer[8];
    if (byteCount != numRegisters * 2) {
        DTU_LOGE("Unexpected byte count in response: %d", byteCount);
        return false;
    }

    // Extract register values (big-endian)
    for (uint16_t i = 0; i < numRegisters; i++) {
        values[i] = (buffer[9 + (i * 2)] << 8) | buffer[10 + (i * 2)];
    }

    DTU_LOGD("Received Modbus response successfully, %d registers", numRegisters);
    return true;
}

bool Provider::readModbusRegister(const PowerMeterModbusTcpRegisterConfig& regConfig, float& value)
{
    if (regConfig.Address == 0) {
        return false; // Skip if register not configured
    }

    if (!connectToDevice()) {
        return false;
    }

    // Always use input registers (function code 0x04)
    uint8_t functionCode = 0x04; // Read Input Registers

    // Determine number of registers to read based on data type
    uint16_t numRegisters = 1;
    if (regConfig.DataType == PowerMeterModbusTcpRegisterConfig::RegisterDataType::INT32 ||
        regConfig.DataType == PowerMeterModbusTcpRegisterConfig::RegisterDataType::UINT32 ||
        regConfig.DataType == PowerMeterModbusTcpRegisterConfig::RegisterDataType::FLOAT) {
        numRegisters = 2; // 32-bit types need 2 registers
    }

    if (!sendModbusRequest(_cfg.DeviceId, functionCode, regConfig.Address, numRegisters)) {
        return false;
    }

    uint16_t rawValues[2];
    if (!receiveModbusResponse(rawValues, numRegisters)) {
        return false;
    }

    // Convert based on data type
    float rawValue = 0.0f;
    switch (regConfig.DataType) {
        case PowerMeterModbusTcpRegisterConfig::RegisterDataType::INT16:
            rawValue = static_cast<float>(static_cast<int16_t>(rawValues[0]));
            break;
        case PowerMeterModbusTcpRegisterConfig::RegisterDataType::UINT16:
            rawValue = static_cast<float>(rawValues[0]);
            break;
        case PowerMeterModbusTcpRegisterConfig::RegisterDataType::INT32: {
            // Combine two 16-bit registers into 32-bit (big-endian)
            int32_t combined = (static_cast<int32_t>(rawValues[0]) << 16) | rawValues[1];
            rawValue = static_cast<float>(combined);
            break;
        }
        case PowerMeterModbusTcpRegisterConfig::RegisterDataType::UINT32: {
            // Combine two 16-bit registers into 32-bit (big-endian)
            uint32_t combined = (static_cast<uint32_t>(rawValues[0]) << 16) | rawValues[1];
            rawValue = static_cast<float>(combined);
            break;
        }
        case PowerMeterModbusTcpRegisterConfig::RegisterDataType::FLOAT: {
            // Combine two 16-bit registers into float (big-endian IEEE 754)
            uint32_t combined = (static_cast<uint32_t>(rawValues[0]) << 16) | rawValues[1];
            memcpy(&rawValue, &combined, sizeof(float));
            break;
        }
        default:
            rawValue = static_cast<float>(rawValues[0]);
            break;
    }

    // Apply scaling factor
    value = rawValue * regConfig.ScalingFactor;

    DTU_LOGD("Read register %d (type %d): raw=%f, scaling=%f, value=%f",
             regConfig.Address,
             static_cast<int>(regConfig.DataType),
             rawValue,
             regConfig.ScalingFactor,
             value);
    return true;
}

void Provider::pollingLoop()
{
    std::unique_lock<std::mutex> lock(_pollingMutex);

    while (!_stopPolling) {
        auto elapsedMillis = millis() - _lastPoll;
        auto intervalMillis = _cfg.PollingInterval * 1000;
        if (_lastPoll > 0 && elapsedMillis < intervalMillis) {
            auto sleepMs = intervalMillis - elapsedMillis;
            _cv.wait_for(lock, std::chrono::milliseconds(sleepMs),
                    [this] { return _stopPolling; });
            continue;
        }

        _lastPoll = millis();

        // Reading takes time, so release the lock during I/O operations
        lock.unlock();

        float powerValue = 0.0;
        float powerL1 = 0.0;
        float powerL2 = 0.0;
        float powerL3 = 0.0;
        float voltageL1 = 0.0;
        float voltageL2 = 0.0;
        float voltageL3 = 0.0;
        float importEnergy = 0.0;
        float exportEnergy = 0.0;

        bool success = true;

        // Read power register
        if (_cfg.PowerRegister.Address > 0) {
            success &= readModbusRegister(_cfg.PowerRegister, powerValue);
        }

        // Read per-phase power registers
        if (_cfg.PowerL1Register.Address > 0) {
            success &= readModbusRegister(_cfg.PowerL1Register, powerL1);
        }
        if (_cfg.PowerL2Register.Address > 0) {
            success &= readModbusRegister(_cfg.PowerL2Register, powerL2);
        }
        if (_cfg.PowerL3Register.Address > 0) {
            success &= readModbusRegister(_cfg.PowerL3Register, powerL3);
        }

        // Read voltage registers
        if (_cfg.VoltageL1Register.Address > 0) {
            success &= readModbusRegister(_cfg.VoltageL1Register, voltageL1);
        }
        if (_cfg.VoltageL2Register.Address > 0) {
            success &= readModbusRegister(_cfg.VoltageL2Register, voltageL2);
        }
        if (_cfg.VoltageL3Register.Address > 0) {
            success &= readModbusRegister(_cfg.VoltageL3Register, voltageL3);
        }

        // Read energy registers
        if (_cfg.ImportRegister.Address > 0) {
            success &= readModbusRegister(_cfg.ImportRegister, importEnergy);
        }
        if (_cfg.ExportRegister.Address > 0) {
            success &= readModbusRegister(_cfg.ExportRegister, exportEnergy);
        }

        lock.lock();

        if (!success) {
            DTU_LOGE("Failed to read some Modbus registers");
            continue;
        }

        // Update data points
        {
            auto scopedLock = _dataCurrent.lock();

            if (_cfg.PowerRegister.Address > 0) {
                _dataCurrent.add<DataPointLabel::PowerTotal>(powerValue);
            }

            if (_cfg.PowerL1Register.Address > 0) {
                _dataCurrent.add<DataPointLabel::PowerL1>(powerL1);
            }
            if (_cfg.PowerL2Register.Address > 0) {
                _dataCurrent.add<DataPointLabel::PowerL2>(powerL2);
            }
            if (_cfg.PowerL3Register.Address > 0) {
                _dataCurrent.add<DataPointLabel::PowerL3>(powerL3);
            }

            if (_cfg.VoltageL1Register.Address > 0) {
                _dataCurrent.add<DataPointLabel::VoltageL1>(voltageL1);
            }
            if (_cfg.VoltageL2Register.Address > 0) {
                _dataCurrent.add<DataPointLabel::VoltageL2>(voltageL2);
            }
            if (_cfg.VoltageL3Register.Address > 0) {
                _dataCurrent.add<DataPointLabel::VoltageL3>(voltageL3);
            }

            if (_cfg.ImportRegister.Address > 0) {
                _dataCurrent.add<DataPointLabel::Import>(importEnergy);
            }
            if (_cfg.ExportRegister.Address > 0) {
                _dataCurrent.add<DataPointLabel::Export>(exportEnergy);
            }
        }

        DTU_LOGD("TotalPower: %5.2f", getPowerTotal());
    }
}

} // namespace PowerMeters::Modbus::Tcp
