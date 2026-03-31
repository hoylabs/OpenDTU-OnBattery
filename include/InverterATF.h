// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Locking policy:
 * - Public getters having the prefix 'get' take a shared lock internally.
 * - Public mutating methods take an exclusive lock internally.
 * - Private getters having the prefix 'g' do not take a lock.
 * - Private methods are called with the appropriate lock held by the public methods.
 * - Fetching external data is done before acquiring _mutex. The only exception is data from the configuration or
 *   data used just for visualization.
 * - Flags marked as atomic may be read lock-free.
 */

#pragma once
#include <Arduino.h>
#include <shared_mutex>
#include <atomic>
#include <ArduinoJson.h>


class InverterATF {
    public:
        InverterATF() = default;
        ~InverterATF() = default;
        InverterATF(const InverterATF&) = delete;
        InverterATF& operator=(const InverterATF&) = delete;
        InverterATF(InverterATF&&) = delete;
        InverterATF& operator=(InverterATF&&) = delete;


        // indicates whether ATF is actually active
        bool isATFActive() const { return _useATF.load(); }

        // activate the ATF data structure
        // use a unique lock when calling this method
        bool activateATF(uint16_t const nomPower);

        // deactivate the ATF data structure
        // use a unique lock when calling this method
        void deactivateATF(void);

        // update the ATF data with the current power and limit pair
        // use a unique lock when calling this method
        void setATFData(float const power, float const limit);

        // deserialize the ATF data from the runtime data file
        // use a unique lock when calling this method
        void deserializeATFData(JsonObject obj);

        // serialize the ATF data to the runtime data file
        // use a shared lock when calling this method
        void serializeATFData(JsonObject obj) const;

        // print the ATF report to the log
        // use a shared lock when calling this method
        void printATFReport(char const* serialStr) const;

        // returns the power for the given limit according to the ATF
        // use a shared/unique lock when calling this method
        uint16_t getATFPower(float const limit) const;

        // returns the limit for the given power according to the ATF
        // use a shared/unique lock when calling this method
        float getATFLimit(uint16_t const power) const;

    private:
        float makeAveragePower(float newValue, float const oldValue);
        enum class State : uint8_t { OFF, DEFAULT_INIT, RTD_INIT };

        std::atomic<bool> _useATF = false;              // false: ATF inactive and no memory allocated for the data array
        State _state = State::OFF;                      // state of the ATF data array
        static constexpr uint8_t _size = 101;           // Fixed size of the ATF data array, never NEVER change the size!
        std::unique_ptr<float[]> _realPower = nullptr;  // ATF data array, index 0-100%

        uint16_t _nomInvPower = 0;                      // Inverter nominal power [W]
        uint16_t _maxInvPower = 0;                      // Inverter maximum power (nominal power * MAX_OVERDRIVE_FACTOR) [W]
        uint16_t _absDiffPower = 0;                     // maximum absolute difference, used for power value checks [W]
        mutable std::pair<uint16_t, float> _cache;      // cache to avoid recalculations, first = power, second = limit
        mutable std::shared_mutex _mutex;               // mutex to protect the shared data

        uint16_t _linearFault = 0;                      // counts the number of range check faults
        uint16_t _averageWarning = 0;                   // counts the number of average warnings
        std::pair<uint16_t, float> _linearPairFault;    // buffer the maximum range check fault
        std::pair<float, float> _averagePairWarning;    // buffer the maximum average check warning
};
