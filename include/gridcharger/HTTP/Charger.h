// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <atomic>
#include <optional>
#include <string>
#include <Configuration.h>
#include <gridcharger/HTTP/DataPoints.h>

namespace GridChargers::HTTP {

// a single AC charger connected to a smart plug which is switched and
// read via HTTP requests.
class Charger {
public:
    Charger(size_t index, GridChargerHTTPConfig const& sharedConfig);

    bool init();

    size_t getIndex() const { return _index; }
    std::string const& getName() const { return _name; }
    float getNominalPower() const { return _nominalPower; }

    // the commanded state, confirmed to be "on" if power is measured
    bool getPowerState() const { return _powerState; }

    // true if the minimum on/off time since the last switch has elapsed
    bool isSwitchAllowed() const;

    // send the respective HTTP request. returns false on failure, in which
    // case the power state is not changed.
    bool powerOn();
    bool powerOff();

    // reads the current AC power from the smart plug. called by the polling task.
    void poll();

    // the most recent AC power reading, if it is not outdated
    std::optional<float> getAcPower() const;
    bool isDataFresh() const;
    uint32_t getLastUpdate() const { return _dataCurrent.getLastUpdate(); }

    static constexpr uint32_t MIN_SWITCH_INTERVAL_MS = 60 * 1000;
    static constexpr uint32_t DATA_MAX_AGE_MS = 30 * 1000;

private:
    bool sendRequest(String const& url);
    bool performRequest(String const& url);
    std::optional<float> readPower();

    static constexpr uint16_t HTTP_TIMEOUT_MS = 5000;
    static constexpr uint32_t RETRY_BACKOFF_MS = 10 * 1000;

    size_t const _index;
    std::string _name;
    String _baseUrl;
    String _uriOn;
    String _uriOff;
    String _uriStats;
    String _powerParam;
    float _nominalPower;

    std::atomic<bool> _powerState{false};

    // only accessed from the main loop
    bool _switchedOnce = false;
    uint32_t _lastSwitchMillis = 0;
    bool _lastRequestFailed = false;
    uint32_t _lastRequestFailedMillis = 0;

    DataPointContainer _dataCurrent;
};

} // namespace GridChargers::HTTP
