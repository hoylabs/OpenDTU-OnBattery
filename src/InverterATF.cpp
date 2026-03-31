// SPDX-License-Identifier: GPL-2.0-or-later

/* Inverter transfer function correction / InverterATF (Inverter Adaptive Transfer Function)
 *
 * The linear transfer function "Limit = m * Power" of the inverter is not always linear. Especially at low power values
 * (below 10%) and high power values (above 90%) in combination with 24VDC, the inverter often shows a non linear behavior.
 * The negative effects are: Oscillations of the power, reduced efficiency and forcing the inverter into standby mode.
 * The problem can be solved with an adaptive transfer function (ATF).
 *
 * 30.01.2025 - 0.1 - First version
 * 06.03.2025 - 0.2 - Double search to find the left and right value and keep the array increasing
 * 30.10.2025 - 0.3 - Use 'Runtime Data' to store the correction table, add shared mutex for thread safety
 * 27.11.2025 - 0.4 - Improved range checks and averaging of new values
 */

#include <LogHelper.h>
#include "InverterATF.h"

#undef TAG
static const char* TAG = "dynamicPowerLimiter";
static const char* SUBTAG = "ATF";

static constexpr float MAX_OVERDRIVE_FACTOR = 1.07f; // 7% maximum overdrive factor, on 600W inverters 642W are possible
static constexpr float MAX_POWER_TOLL = 1.15f;       // maximum factor [%] for the power value compared to the correction table
static constexpr float MAX_LINEAR_DIFF_P = 1.50f;    // 50% maximum difference of the measured power value compared to the linear value
static constexpr float MAX_LINEAR_DIFF_A = 0.05f;    // 5% of nominal power maximum difference compared to the linear value


// activate the following line to enable debug
// Hint: And also disable the DPL
//#define DEBUG_LOGGING


/*
 * activate ATF data array with linear values
 */
bool InverterATF::activateATF(uint16_t const nomPower) {

    std::unique_lock<std::shared_mutex> lock(_mutex);

    if (nomPower == 0) { return false; }
    if (_realPower != nullptr) { return true; } // already initialized

    // allocate memory for the ATF data array
    _realPower = std::make_unique<float[]>(_size);
    if (_realPower == nullptr) {
        DTU_LOGE("Unable to allocate memory for ATF data array");
        return false;
    }

    _nomInvPower = nomPower;
    _maxInvPower = _nomInvPower * MAX_OVERDRIVE_FACTOR;
    _absDiffPower = static_cast<uint16_t>( _nomInvPower * MAX_LINEAR_DIFF_A);
    _linearFault = 0;
    _averageWarning = 0;
    _averagePairWarning = { 0.0f, 0.0f };
    _linearPairFault = { 0, 0.0f };
    _cache = { 0, 0.0f };

    // init the ATF data array with 1% power steps
    auto powerStep = static_cast<float>( _nomInvPower ) / (_size - 1);
    for (auto idx = 0; idx < _size; ++idx) {
        _realPower[idx] = powerStep * idx;
    }

    _state = State::DEFAULT_INIT;
    _useATF.store(true);
    return true;
}


/*
 * deactivate ATF
 */
void InverterATF::deactivateATF() {

    std::unique_lock<std::shared_mutex> lock(_mutex);

    _realPower.reset();
    _nomInvPower = 0;

    _state = State::OFF;
    _useATF.store(false);
}


/*
 * Update the correction array with a new pair, power [W] and limit [%].
 * Checks whether the values ​​are valid, calculates an average value, and ensures the consistency of the data.
 */
void InverterATF::setATFData(float const power, float const limit) {

    if (!_useATF.load()) { return; }

    std::unique_lock<std::shared_mutex> lock(_mutex);

    // basic range checks
    if ((limit <= 0.0f) || (limit > 100.0f) || (power <= 0.0f) || (power > _maxInvPower )) { return; }

    // validation check, values must be in an range of +/-35% or +/-12W compared to the linear value
    auto linearPower = (_nomInvPower / 100.0f) * limit;
    auto minLinPower = linearPower / MAX_LINEAR_DIFF_P;
    auto maxLinPower = linearPower * MAX_LINEAR_DIFF_P;
    if (((power < minLinPower) || (power > maxLinPower)) && (fabs(power - linearPower) > _absDiffPower)) {

        // logging of the biggest difference compared to linear value
        _linearFault++;
        if (fabs(power - linearPower) > fabs((_nomInvPower / 100.0f * _linearPairFault.second) - _linearPairFault.first)) {
            _linearPairFault = { static_cast<uint16_t>(power), limit };
        }
        return; // out of range, ignore the new values
    }

    _cache = { power, limit } ; // fill the cache
    auto idxL = static_cast<uint8_t>(limit); // remove decimal part
    auto idxR = idxL;
    if  ((limit - idxL) != 0.0f) { idxR = idxL + 1; } // we are between two indexes

    // Note: additional range checks are not necessary, because limit is between 0.1% and 100%
    // so idxL is between 0 and 100, and idxR is between 1 and 100
    auto powerL = _realPower[idxL];
    auto powerR = _realPower[idxR];
    auto newL = power;

    #ifdef DEBUG_LOGGING
    DTU_LOGI("Limit: %0.1f%%, Power: %0.1fW", limit, power);
    DTU_LOGI("Old values left: %u%%, %0.1fW, Old values right: %u%%, %0.1fW", idxL, powerL, idxR, powerR);
    #endif

    if (idxL != idxR ) {
        // we are between two indexes, so we calculate the new power values for the left and right index
        float oldPower = powerL + (powerR - powerL) * (limit - idxL);
        if (oldPower <= 0.0f) { oldPower = 1.0f; } // avoid division by zero
        auto diffFactor = power / oldPower - 1.0f;

        // we calculate the index values (left and right)
        if (powerL > 1.0f) {
            newL = powerL * ( 1.0f + diffFactor * (1.0f - (limit - idxL)));
        } else {
            if (idxL != 0) { newL = power / 2; } // if the left power is too small, we just set it to half of the new power
        }
        auto newR = powerR * ( 1.0f + diffFactor * (1.0f - (idxR - limit)));

        _realPower[idxR] = makeAveragePower(newR, _realPower[idxR]);
    }
    _realPower[idxL] = makeAveragePower(newL, _realPower[idxL]);

    #ifdef DEBUG_LOGGING
    DTU_LOGI("New values left: %u%%, %0.1fW, New values right: %u%%, %0.1fW", idxL, _realPower[idxL], idxR, _realPower[idxR]);
    #endif

    // we keep the array in an equal or increasing order to avoid problems if we calculate
    // the limit from the power. We check the left and the right side of the array
    // beginning from the closer changed index
    auto idxStart = ((limit - idxL) <= 0.5f) ? idxL : idxR;

    #ifdef DEBUG_LOGGING
    DTU_LOGI("Array check start index: %u%%", idxStart);
    #endif

    // check if the right side from the start index is equal or increasing
    for (auto idx = idxStart; idx + 1 < _size; ++idx) {
        if (_realPower[idx] > _realPower[idx + 1]) {
            _realPower[idx + 1] = _realPower[idx];
        } else {
            if (idx > idxStart + 1) { break; } // we can stop if we are at least 2 indexes away from the start
        }
    }

    // check if the left side from the start index is equal or decreasing
    for (auto idx = idxStart; idx > 0; --idx) {
        if (_realPower[idx - 1] > _realPower[idx]) {
            _realPower[idx - 1] = _realPower[idx];
        } else {
            if (idx < idxStart - 1) { break; } // we can stop if we are at least 2 indexes away from the start
        }
    }
}


/*
 * Check and limit the new value to +/- 15% of the current value
 */
float InverterATF::makeAveragePower(float newValue, float const oldValue) {
    float minValue = oldValue / MAX_POWER_TOLL;
    float maxValue = oldValue * MAX_POWER_TOLL;

    auto actWarning = _averageWarning;
    if (newValue < minValue) {
        newValue = minValue;
        _averageWarning++;
    } else if (newValue > maxValue) {
        newValue = maxValue;
        _averageWarning++;
    }

    if (_averageWarning > actWarning) {
        auto faultOld = std::abs(_averagePairWarning.first - _averagePairWarning.second);
        auto faultNew = std::abs(newValue - oldValue);
        if (faultNew > faultOld) {
            _averagePairWarning = { newValue, oldValue };
        }
    }

    auto back = (newValue + oldValue) / 2.0f;
    if (back > _maxInvPower) { back = static_cast<float>(_maxInvPower); }
    return back; // return the average
}


/*
 * Get the power [W] of the given limit [%]
 * If the limit is between two values, then we do a linear interpolation
 */
uint16_t InverterATF::getATFPower(float const limit) const {

    if (!_useATF.load()) { return 0; }

    {
        std::shared_lock<std::shared_mutex> slock(_mutex);

        if (limit <= 0.0f) { return 0; }
        if (limit >= 100.0f) { return static_cast<uint16_t>(_realPower[_size - 1]); }

        // shortcut, check the cache, if the new limit is the same as the last limit
        if (limit == _cache.second) { return _cache.first; }

    } // end of shared lock

    // cache miss -> unique lock to compute and update cache
    std::unique_lock<std::shared_mutex> lock(_mutex);

    // re-check under exclusive lock
    if (limit == _cache.second) { return _cache.first; }

    _cache.second = limit; // store the new limit in the cache
    auto idx = static_cast<uint8_t>(limit); // remove decimal part

    // idx is between 0 and 99, so idx+1 is between 1 and 100, safe to access the array without further range check
    float power = 0.0f;
    if (limit == static_cast<float>(idx)) {
        power = _realPower[idx]; // exact match
    } else {
        float m = (_realPower[idx + 1] - _realPower[idx]);   // 1% step
        power = _realPower[idx] + m * (limit - static_cast<float>(idx));
    }

    #ifdef DEBUG_LOGGING
    DTU_LOGI("Given limit: %0.1f%% --> Calculated power: %0.0fW", limit, power);
    #endif

    _cache.first = static_cast<uint16_t>(power + 0.5f); // round to nearest integer
    return _cache.first;
}


/*
 * Get the limit [%] of the given power [W]
 * If the limit is between two values, then we do a linear interpolation
 */
float InverterATF::getATFLimit(uint16_t const power) const {

    if (!_useATF.load()) { return 0.0f; }

    {
        std::shared_lock<std::shared_mutex> slock(_mutex);

        if (power <= 0) { return 0.0f; }
        if (power >= static_cast<uint16_t>(_realPower[_size - 1])) { return 100.0f; }

        // shortcut, check the cache, if the new power is the same as the last power
        if (power == _cache.first) { return _cache.second; }

    } // end of shared lock

    // cache miss -> unique lock to compute and update cache
    std::unique_lock<std::shared_mutex> lock(_mutex);

    // re-check under exclusive lock
    if (power == _cache.first) { return _cache.second; }

    _cache.first = power; // store the new power value in the cache

    // search from 0W to max W (index 1 to 100) to find the left value
    auto fPower = static_cast<float>(power);
    uint8_t idxLeft = 0;
    for (uint8_t idx = 1; idx < _size; ++idx) {
        if (fPower <= _realPower[idx]) {
            idxLeft = idx - 1;
            break;
        }
    }
    if (idxLeft >= _size) { idxLeft = _size - 1; } // safety check
    auto limit = static_cast<float>(idxLeft);

    // linear interpolation is not necessary if
    // the left power value matches the given power or if we reached the end of the array
    if ((power != static_cast<uint16_t>(_realPower[idxLeft] + 0.5f)) && (idxLeft < _size - 1)) {

        // linear interpolation between the left and right value
        auto const& powLeft = _realPower[idxLeft];
        auto const& powRight = _realPower[idxLeft + 1];

        // safety check to avoid division by zero and use of invalid values
        if ( powLeft < powRight) {
            auto m = 1.0f / (powRight - powLeft);
            limit = limit + m * (power - powLeft);
            if (limit < 0.0f) { limit = 0.0f; }
            if (limit > 100.0f) { limit = 100.0f; }
        }
    }

    #ifdef DEBUG_LOGGING
    DTU_LOGI("Given power: %iW --> Calculated limit: %0.1f%%", power, limit);
    #endif

    _cache.second = roundf(limit * 10.0f) / 10.0f; // round to 1 decimal place
    return _cache.second;
}


/*
 * Serialize the ATF data array to be written into the runtime file
 */
void InverterATF::serializeATFData(JsonObject obj) const {

    if (!_useATF.load()) { return; }

    std::shared_lock<std::shared_mutex> lock(_mutex);

    JsonArray data = obj["data"].to<JsonArray>();
    for (auto idx = 0; idx < _size; ++idx) {
	    data.add(roundf(_realPower[idx] * 10.0f) / 10.0f); // round to 1 decimal place
    }
}


/*
 * Deserialize ATF data array from the runtime file
 */
void InverterATF::deserializeATFData(JsonObject obj) {

    if (!_useATF.load()) { return; }

    std::unique_lock<std::shared_mutex> lock(_mutex);

    if (_state == State::RTD_INIT) { return; } // already initialized from runtime file

    // if the data from the runtime file does not exist or the size is not identical,
    // then we do not copy any values and keep the default values
    JsonArray data = obj["data"];
    if (data.size() == _size) {
        auto idx = 0;
        for (JsonVariantConst value : data) {
            float newValue = value.as<float>();

            // range checks
            if (newValue < 0.0f) { newValue = 0.0f; }
            if (newValue > _maxInvPower) { newValue = _maxInvPower; }

            _realPower[idx] = newValue;

            ++idx;
        }
        _state = State::RTD_INIT;
    }
}


/*
 * Print the correction report to the log
 */
void InverterATF::printATFReport(char const* serialStr) const {

    if (!_useATF.load()) { return; }

    std::shared_lock<std::shared_mutex> lock(_mutex);

    DTU_LOGI("");
    DTU_LOGI("------------------------- ATF Report --------------------------");
    DTU_LOGI("Inverter serial: %s, Nominal power: %uW", serialStr, _nomInvPower);
    DTU_LOGI("Data state: %s", (_state == State::RTD_INIT) ? "Initialized by runtime values" : "Initialized by default values");
    DTU_LOGI("Data out of range faults: %i [Limit: %0.1f%%, AFT: %uW, Linear: %uW]", _linearFault, _linearPairFault.second,
        _linearPairFault.first, static_cast<uint16_t>(_nomInvPower / 100.0f * _linearPairFault.second));
    DTU_LOGI("Data average warnings: %i [New: %0.1fW, Old: %0.1fW]", _averageWarning, _averagePairWarning.first, _averagePairWarning.second);

    bool error = false;
    for (auto idx = 1; idx < _size; ++idx) {
        if (_realPower[idx-1] > _realPower[idx]) { error = true; break; }
    }
    DTU_LOGI("Data array %s increasing", error ? "is not" : "is");
    DTU_LOGI("");

    size_t constexpr columnWidth = 5;
    size_t constexpr columnMaxNr = 20;
    size_t constexpr rowMaxNr = 5;
    static constexpr size_t  kTableWidth = columnMaxNr * columnWidth + 10;

    char cBuffer[kTableWidth];

    for (auto row = 0; row < rowMaxNr; ++row) {
        for (auto kind = 0; kind < 2; ++kind) {
            char *cBuf = cBuffer;
            auto remaining = sizeof(cBuffer);
            for (auto idx = 1; idx <= columnMaxNr; ++idx) {
                auto globalIdx = row * columnMaxNr + idx;
                if (kind == 0) {
                    if (globalIdx < _size) {
                        snprintf(cBuf, remaining, " %2i%%,", static_cast<int>(globalIdx)); // Limit
                    } else {
                        snprintf(cBuf, remaining, "   ,");
                    }
                } else {
                    if (globalIdx < _size) {
                        snprintf(cBuf, remaining, " %3.0f,", _realPower[globalIdx]); // Power
                    } else {
                        snprintf(cBuf, remaining, "   ,");
                    }
                }
                cBuf += columnWidth;
                remaining -= columnWidth;
            }
            DTU_LOGI("%s", cBuffer);
        }
    }

    DTU_LOGI("---------------------------------------------------------------");
    DTU_LOGI("");
}
