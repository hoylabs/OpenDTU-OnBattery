// SPDX-License-Identifier: GPL-2.0-or-later

#include <gridcharger/HTTP/Charger.h>
#include <HttpGetter.h>
#include <Utils.h>
#include <LogHelper.h>

#undef TAG
static const char* TAG = "gridCharger";
static const char* SUBTAG = "HTTP";

namespace GridChargers::HTTP {

Charger::Charger(size_t index, GridChargerHTTPConfig const& sharedConfig)
    : _index(index)
{
    auto const& cfg = sharedConfig.Chargers[index];
    _name = cfg.Name;
    _baseUrl = cfg.Url;
    _uriOn = _baseUrl + sharedConfig.UriOn;
    _uriOff = _baseUrl + sharedConfig.UriOff;
    _uriStats = _baseUrl + sharedConfig.UriStats;
    _powerParam = sharedConfig.UriPowerParam;
    _nominalPower = cfg.AcPower;
}

bool Charger::init()
{
    if (_name.empty()) { _name = "Charger " + std::to_string(_index + 1); }

    if (_baseUrl.isEmpty()) {
        DTU_LOGE("[%s] URL is empty", _name.c_str());
        return false;
    }

    for (auto const* url : { &_uriOn, &_uriOff, &_uriStats }) {
        if (url->length() >= HTTP_REQUEST_MAX_URL_STRLEN) {
            DTU_LOGE("[%s] Combined URL %s exceeds %u chars and would be truncated",
                    _name.c_str(), url->c_str(), HTTP_REQUEST_MAX_URL_STRLEN);
            return false;
        }
    }

    DTU_LOGI("[%s] %s, %.0fW", _name.c_str(), _baseUrl.c_str(), _nominalPower);
    return true;
}

bool Charger::isSwitchAllowed() const
{
    return !_switchedOnce || (millis() - _lastSwitchMillis) >= MIN_SWITCH_INTERVAL_MS;
}

bool Charger::powerOn()
{
    if (!sendRequest(_uriOn)) { return false; }
    _powerState = true;
    _switchedOnce = true;
    _lastSwitchMillis = millis();
    DTU_LOGI("[%s] Power ON", _name.c_str());
    return true;
}

bool Charger::powerOff()
{
    if (!sendRequest(_uriOff)) { return false; }
    _powerState = false;
    _switchedOnce = true;
    _lastSwitchMillis = millis();
    DTU_LOGI("[%s] Power OFF", _name.c_str());
    return true;
}

bool Charger::sendRequest(String const& url)
{
    // an unreachable smart plug would otherwise block the main loop for the
    // full timeout on every iteration while the switch request is retried.
    if (_lastRequestFailed && (millis() - _lastRequestFailedMillis) < RETRY_BACKOFF_MS) {
        return false;
    }

    bool success = performRequest(url);
    _lastRequestFailed = !success;
    if (!success) { _lastRequestFailedMillis = millis(); }
    return success;
}

bool Charger::performRequest(String const& url)
{
    HttpRequestConfig httpRequest{};
    strlcpy(httpRequest.Url, url.c_str(), sizeof(httpRequest.Url));
    httpRequest.Timeout = HTTP_TIMEOUT_MS;
    auto httpGetter = std::make_unique<HttpGetter>(httpRequest);
    DTU_LOGD("[%s] Sending request to %s", _name.c_str(), url.c_str());

    if (!httpGetter->init()) {
        DTU_LOGE("[%s] ERROR INIT HttpGetter %s", _name.c_str(), httpGetter->getErrorText());
        return false;
    }

    if (!httpGetter->performGetRequest()) {
        DTU_LOGE("[%s] ERROR GET HttpGetter %s", _name.c_str(), httpGetter->getErrorText());
        return false;
    }

    return true;
}

std::optional<float> Charger::readPower()
{
    HttpRequestConfig httpRequest{};
    strlcpy(httpRequest.Url, _uriStats.c_str(), sizeof(httpRequest.Url));
    httpRequest.Timeout = HTTP_TIMEOUT_MS;
    auto httpGetter = std::make_unique<HttpGetter>(httpRequest);
    DTU_LOGD("[%s] Reading from %s", _name.c_str(), _uriStats.c_str());

    if (!httpGetter->init()) {
        DTU_LOGE("[%s] ERROR INIT HttpGetter %s", _name.c_str(), httpGetter->getErrorText());
        return std::nullopt;
    }

    httpGetter->addHeader("Content-Type", "application/json");
    httpGetter->addHeader("Accept", "application/json");

    auto res = httpGetter->performGetRequest();
    if (!res) {
        DTU_LOGE("[%s] ERROR GET HttpGetter %s", _name.c_str(), httpGetter->getErrorText());
        return std::nullopt;
    }

    auto pStream = res.getStream();
    if (!pStream) {
        DTU_LOGE("Programmer error: HTTP request yields no stream");
        return std::nullopt;
    }

    JsonDocument jsonResponse;
    const DeserializationError error = deserializeJson(jsonResponse, *pStream);
    if (error) {
        DTU_LOGE("[%s] Unable to parse server response as JSON: %s", _name.c_str(), error.c_str());
        return std::nullopt;
    }

    auto pathResolutionResult = Utils::getJsonValueByPath<float>(jsonResponse, _powerParam);
    if (!pathResolutionResult.second.isEmpty()) {
        DTU_LOGE("[%s] ERROR reading AC power from smart plug: %s",
                _name.c_str(), pathResolutionResult.second.c_str());
        return std::nullopt;
    }

    return pathResolutionResult.first;
}

void Charger::poll()
{
    // on failure the previous reading is kept with its original timestamp,
    // such that outdated data can be detected by the power control loop.
    auto oPower = readPower();
    if (!oPower) { return; }

    DTU_LOGV("[%s] AC power: %.1fW", _name.c_str(), *oPower);

    // One-way confirmation: a non-zero measurement proves the charger is
    // drawing power, but a zero reading does not prove it is off (battery
    // may be full, or the charger may not yet have ramped up). The
    // commanded state is otherwise maintained by powerOn()/powerOff().
    if (*oPower > 0) { _powerState = true; }

    auto scopedLock = _dataCurrent.lock();
    _dataCurrent.add<DataPointLabel::AcPower>(*oPower);
}

bool Charger::isDataFresh() const
{
    auto lastUpdate = getLastUpdate();
    return lastUpdate > 0 && (millis() - lastUpdate) < DATA_MAX_AGE_MS;
}

std::optional<float> Charger::getAcPower() const
{
    if (!isDataFresh()) { return std::nullopt; }
    return _dataCurrent.get<DataPointLabel::AcPower>();
}

} // namespace GridChargers::HTTP
