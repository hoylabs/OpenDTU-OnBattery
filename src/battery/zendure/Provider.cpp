#include <functional>
#include <Configuration.h>
#include <battery/zendure/Provider.h>
#include <MqttSettings.h>
#include <SunPosition.h>
#include <Utils.h>
#include <LogHelper.h>

#undef TAG
static const char* TAG = "battery";
static const char* SUBTAG = "Zendure";

namespace Batteries::Zendure {

Provider::Provider()
    : _stats(std::make_shared<Stats>())
    , _hassIntegration(std::make_shared<HassIntegration>(_stats)) { }

bool Provider::init()
{
    auto const& config = Configuration.get();
    String deviceType = String();

    if (strlen(config.Battery.Zendure.DeviceId) != 8) {
        DTU_LOGE("Invalid device id '%s'!", config.Battery.Zendure.DeviceId);
        return false;
    }

    _stats->setManufacturer("Zendure");

    _topicPersistentSettings = MqttSettings.getPrefix() + "battery/persistent/";

    auto topic = _topicPersistentSettings + "#";
    MqttSettings.subscribe(topic, 0/*QoS*/,
            std::bind(&Provider::onMqttMessagePersistentSettings,
                this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4)
            );
    DTU_LOGD("Subscribed to '%s' for persistent settings", topic.c_str());

    _rateFullUpdateMs   = Configuration.get().Battery.Zendure.PollingInterval * 1000;
    _nextFullUpdate     = millis() + _rateFullUpdateMs / 2;
    _rateTimesyncMs     = ZENDURE_SECONDS_TIMESYNC * 1000;
    _nextTimesync       = _nextFullUpdate;
    _rateSunCalcMs      = ZENDURE_SECONDS_SUNPOSITION * 1000;
    _nextSunCalc        = millis() + _rateSunCalcMs / 2;

    return true;
}

void Provider::deinit()
{
    if (!_topicPersistentSettings.isEmpty()) {
        MqttSettings.unsubscribe(_topicPersistentSettings + "#");
        _topicPersistentSettings.clear();
    }
}

void Provider::loop()
{
    auto ms = millis();
    auto const& config = Configuration.get();
    const bool isDayPeriod = SunPosition.isSunsetAvailable() ? SunPosition.isDayPeriod() : true;
    auto const chargeThroughState = _stats->_charge_through_state.value_or(ChargeThroughState::Disabled);

    // if auto shutdown is enabled and battery switches to idle at night, turn off status requests to prevent keeping battery awake
    if (config.Battery.Zendure.AutoShutdown && !isDayPeriod && _stats->_state == State::Idle) {
        return;
    }

    // check if we run in schedule mode
    if (ms >= _nextSunCalc) {
        _nextSunCalc = ms + _rateSunCalcMs;

        calculateFullChargeAge();

        struct tm timeinfo_local;
        struct tm timeinfo_sun;
        if (getLocalTime(&timeinfo_local, 5)) {
            std::time_t current = std::mktime(&timeinfo_local);

            std::time_t sunrise = 0;
            std::time_t sunset = 0;

            if (SunPosition.sunriseTime(&timeinfo_sun)) {
                sunrise = std::mktime(&timeinfo_sun) + config.Battery.Zendure.SunriseOffset * 60;
            }

            if (SunPosition.sunsetTime(&timeinfo_sun)) {
                sunset = std::mktime(&timeinfo_sun) + config.Battery.Zendure.SunsetOffset * 60;
            }

            if (sunrise && sunset) {
                // check charge-through at sunrise (make sure its triggered at least once)
                if (current > sunrise && current < (sunrise + ZENDURE_SECONDS_SUNPOSITION + ZENDURE_SECONDS_SUNPOSITION/2)) {
                    // Calculate expected daylight to asure charge through starts in the morning if sheduled for this day
                    // We just use the time between rise and set, as we do not know anything about the actual conditions,
                    // we can only expect that there will be NO sun between sunset and sunrise ;)
                    uint32_t maxDaylightHours = (sunset - sunrise + 1800U) / 3600U;
                    checkChargeThrough(maxDaylightHours);
                }

                // running in appointment mode - set outputlimit accordingly
                if (config.Battery.Zendure.OutputControl == BatteryZendureConfig::OutputControl_t::ControlSchedule && chargeThroughState != ChargeThroughState::Hard) {
                    if (current >= sunrise && current < sunset) {
                        setOutputLimit(min(config.Battery.Zendure.MaxOutput, config.Battery.Zendure.OutputLimitDay));
                    } else if (current >= sunset || current < sunrise) {
                        setOutputLimit(min(config.Battery.Zendure.MaxOutput, config.Battery.Zendure.OutputLimitNight));
                    }
                }
            }


        }

        // ensure charge through settings
        switch (chargeThroughState) {
            case ChargeThroughState::Soft:
            case ChargeThroughState::Keep:
                setTargetSoCs(config.Battery.Zendure.MinSoC, 100);
                setBypassMode(BatteryZendureConfig::BypassMode_t::AlwaysOff);
                setOutputLimit(config.Battery.Zendure.OutputLimit);
                break;
            case ChargeThroughState::Hard:
                setTargetSoCs(config.Battery.Zendure.MinSoC, 100);
                setBypassMode(BatteryZendureConfig::BypassMode_t::AlwaysOff);
                setOutputLimit(0);
                break;
            default:
                setTargetSoCs(config.Battery.Zendure.MinSoC, config.Battery.Zendure.MaxSoC);
                setBypassMode(config.Battery.Zendure.BypassMode);
                setOutputLimit(config.Battery.Zendure.OutputLimit);
                break;
        }

    }

    if (!_topicRead.isEmpty()) {
        if (!_payloadFullUpdate.isEmpty() && ms >= _nextFullUpdate) {
            _nextFullUpdate = ms + _rateFullUpdateMs;
            MqttSettings.publishGeneric(_topicRead, _payloadFullUpdate, false, 0);
        }
    }

    if (ms >= _nextTimesync) {
        _nextTimesync = ms + _rateTimesyncMs;
        timesync();

        // update settings (will be skipped if unchanged)
        setInverterMax(config.Battery.Zendure.MaxOutput);

        // republish settings - just to be sure
        writeSettings();
    }
}

void Provider::calculateFullChargeAge()
{
    time_t now;
    if (!Utils::getEpoch(&now)) {
        return;
    }

    if(_stats->_last_full_timestamp.has_value()) {
        auto last_full = *(_stats->_last_full_timestamp);
        uint32_t age = now > last_full  ? (now - last_full) / 3600U : 0U;

        DTU_LOGD("Now: %ld, LastFull: %" PRIu64 ", Diff: %" PRIu32, now, last_full, age);

        // store for webview
        _stats->_last_full_hours = age;
    }

    if(_stats->_last_empty_timestamp.has_value()) {
        auto last_empty = *(_stats->_last_empty_timestamp);
        uint32_t age = now > last_empty  ? (now - last_empty) / 3600U : 0U;

        DTU_LOGD("Now: %ld, LastEmpty: %" PRIu64 ", Diff: %" PRIu32, now, last_empty, age);

        // store for webview
        _stats->_last_empty_hours = age;
    }

}

void Provider::checkChargeThrough(uint32_t predictHours /* = 0 */)
{
    auto const& config = Configuration.get();
    if (!config.Battery.Zendure.ChargeThroughEnable) {
        return;
    }

    // hard charge through will start after configured interval (given in hours)
    auto hardChargeThrough = config.Battery.Zendure.ChargeThroughInterval;

    // soft charge through will be triggered one day (aka. 24 hours) before hard charge through
    auto softChargeThrough = hardChargeThrough - 24;

    auto currentValue      = _stats->_last_full_hours.value_or(0) + predictHours;
    auto noValue           = !_stats->_last_full_timestamp.has_value();

    if (noValue || currentValue > hardChargeThrough) {
        return setChargeThroughState(ChargeThroughState::Hard);
    }

    if (noValue || currentValue > softChargeThrough) {
        return setChargeThroughState(ChargeThroughState::Soft);
    }

    // force IDLE state to prevent sticking at KEEP state
    setChargeThroughState(ChargeThroughState::Idle);
}

void Provider::setTargetSoCs(const float soc_min, const float soc_max)
{
    if (_topicWrite.isEmpty() || !alive()) {
        return;
    }

    if (_stats->_soc_min.value_or(100) != soc_min || _stats->_soc_max.value_or(0) != soc_max) {
        publishProperties(_topicWrite, ZENDURE_REPORT_MIN_SOC, String(soc_min * 10, 0), ZENDURE_REPORT_MAX_SOC, String(soc_max * 10, 0));
        DTU_LOGD("Setting target minSoC from %.1f %% to %.1f %% and target maxSoC from %.1f %% to %.1f %%",
                _stats->_soc_min.value_or(100), soc_min, _stats->_soc_max.value_or(0), soc_max);
    }
}

uint16_t Provider::calcOutputLimit(uint16_t limit) const
{
    if (limit >= 100 || limit == 0 ) {
        return limit;
    }

    uint16_t base = limit / 30U;
    uint16_t remain = (limit % 30U) / 15U;
    return 30 * base + 30 * remain;
}

uint16_t Provider::setOutputLimit(uint16_t limit) const
{
    auto const& config = Configuration.get();

    if (config.Battery.Zendure.OutputControl == BatteryZendureConfig::OutputControl_t::ControlNone ||
        _topicWrite.isEmpty() || !alive() ) {
        return _stats->_output_limit.value_or(0);
    }

    // keep limit below MaxOutput
    limit = min(config.Battery.Zendure.MaxOutput, limit);

    if (_stats->_output_limit != limit) {
        limit = calcOutputLimit(limit);
        publishProperty(_topicWrite, ZENDURE_REPORT_OUTPUT_LIMIT, String(limit));
        DTU_LOGD("Adjusting outputlimit from %" PRIu16 " W to %" PRIu16 " W", _stats->_output_limit.value_or(0), limit);
    }

    return limit;
}

uint16_t Provider::setInverterMax(uint16_t limit) const
{
    if (_topicWrite.isEmpty() || !alive()) {
        return _stats->_inverse_max.value_or(0);
    }

    if (_stats->_inverse_max != limit) {
        limit = calcOutputLimit(limit);
        publishProperty(_topicWrite, ZENDURE_REPORT_INVERSE_MAX_POWER, String(limit));
        DTU_LOGD("Adjusting inverter max output from %" PRIu16 " W to %" PRIu16 " W", _stats->_inverse_max.value_or(0), limit);
    }

    return limit;
}

void Provider::publishProperty(const String& topic, const String& property, const String& value) const
{
    MqttSettings.publishGeneric(topic, "{\"properties\": {\"" + property +  "\": " + value + "} }", false, 0);
}

template<typename... Arg>
void Provider::publishProperties(const String& topic, Arg&&... args) const
{
    static_assert((sizeof...(args) % 2) == 0);

    String out = "{\"properties\": {";
    bool even = true;
    bool first = true;
    for (const String d : std::initializer_list<String>({args...}))
    {
        if (even) {
            if (!first) {
                out += ", ";
            }
            out += "\"" + d + "\": ";
        } else {
            out += d;
        }
        even  = !even;
        first = false;
    }
    out += "} }";
    MqttSettings.publishGeneric(topic, out, false, 0);
}

void Provider::setChargeThroughState(const ChargeThroughState value, const bool publish /* = true */)
{
    if (_stats->_charge_through_state.has_value() && value == *_stats->_charge_through_state) {
        return;
    }

    _stats->_charge_through_state = value;
    DTU_LOGD("Setting charge-through mode to '%s'!", Stats::chargeThroughStateToString(value));
    if (publish) {
        publishPersistentSettings(ZENDURE_PERSISTENT_SETTINGS_CHARGE_THROUGH, String(Stats::chargeThroughStateToString(value)));
    }

    // re-run suncalc to force updates in schedule mode
    rescheduleSunCalc();
}

void Provider::processProperties(std::optional<JsonObjectConst>& props, const uint64_t timestamp)
{
    if (!props.has_value()) { return; }

    _stats->setFirmwareVersion(Utils::getJsonElement<uint32_t>(*props, ZENDURE_REPORT_MASTER_FW_VERSION));
    _stats->setHardwareVersion(Utils::getJsonElement<uint32_t>(*props, ZENDURE_REPORT_MASTER_HW_VERSION));

    auto soc_max = Utils::getJsonElement<float>(*props, ZENDURE_REPORT_MAX_SOC);
    if (soc_max.has_value()) {
        _stats->setSocMax(*soc_max / 10);
    }

    auto soc_min = Utils::getJsonElement<float>(*props, ZENDURE_REPORT_MIN_SOC);
    if (soc_min.has_value()) {
        _stats->setSocMin(*soc_min / 10);
    }

    auto input_limit = Utils::getJsonElement<uint16_t>(*props, ZENDURE_REPORT_INPUT_LIMIT);
    if (input_limit.has_value()) {
        _stats->_input_limit = *input_limit;
    }

    auto output_limit = Utils::getJsonElement<uint16_t>(*props, ZENDURE_REPORT_OUTPUT_LIMIT);
    if (output_limit.has_value()) {
        _stats->setOutputLimit(*output_limit);
    }

    auto inverse_max = Utils::getJsonElement<uint16_t>(*props, ZENDURE_REPORT_INVERSE_MAX_POWER);
    if (inverse_max.has_value()) {
        _stats->_inverse_max = *inverse_max;
    }

    auto state = Utils::getJsonElement<uint8_t>(*props, ZENDURE_REPORT_BATTERY_STATE);
    if (state.has_value() && *state <= 2) {
        _stats->_state = static_cast<State>(*state);
    }

    auto heat_state = Utils::getJsonElement<uint8_t>(*props, ZENDURE_REPORT_HEAT_STATE);
    if (heat_state.has_value()) {
        _stats->_heat_state = static_cast<bool>(*heat_state);
    }

    auto auto_shutdown = Utils::getJsonElement<uint8_t>(*props, ZENDURE_REPORT_AUTO_SHUTDOWN);
    if (auto_shutdown.has_value()) {
        _stats->_auto_shutdown = static_cast<bool>(*auto_shutdown);
    }

    auto auto_recover = Utils::getJsonElement<uint8_t>(*props, ZENDURE_REPORT_AUTO_RECOVER);
    if (auto_recover.has_value()) {
        _stats->setAutoRecover(*auto_recover);
    }

    auto buzzer = Utils::getJsonElement<uint8_t>(*props, ZENDURE_REPORT_BUZZER_SWITCH);
    if (buzzer.has_value()) {
        _stats->_buzzer = static_cast<bool>(*buzzer);
    }

    auto output_power = Utils::getJsonElement<uint16_t>(*props, ZENDURE_REPORT_OUTPUT_POWER);
    if (output_power.has_value()) {
        _stats->setOutputPower(*output_power);
    }

    auto discharge_power = Utils::getJsonElement<uint16_t>(*props, ZENDURE_REPORT_DISCHARGE_POWER);
    if (discharge_power.has_value()) {
        _stats->setDischargePower(*discharge_power);
    }

    auto charge_power = Utils::getJsonElement<uint16_t>(*props, ZENDURE_REPORT_CHARGE_POWER);
    if (charge_power.has_value()) {
        _stats->setChargePower(*charge_power);
    }

    auto solar_power_1 = Utils::getJsonElement<uint16_t>(*props, ZENDURE_REPORT_SOLAR_POWER_MPPT_1);
    if (solar_power_1.has_value()) {
        _stats->setSolarPower1(*solar_power_1);
    }

    auto solar_power_2 = Utils::getJsonElement<uint16_t>(*props, ZENDURE_REPORT_SOLAR_POWER_MPPT_2);
    if (solar_power_2.has_value()) {
        _stats->setSolarPower2(*solar_power_2);
    }

    auto bypass_mode = Utils::getJsonElement<uint8_t>(*props, ZENDURE_REPORT_BYPASS_MODE);
    if (bypass_mode.has_value() && *bypass_mode <= 2) {
        _stats->_bypass_mode = static_cast<BatteryZendureConfig::BypassMode_t>(*bypass_mode);
    }

    auto bypass_state = Utils::getJsonElement<uint8_t>(*props, ZENDURE_REPORT_BYPASS_STATE);
    if (bypass_state.has_value()) {
        _stats->_bypass_state = static_cast<bool>(*bypass_state);
    }

    _stats->_lastUpdate = timestamp;
}

void Provider::processPackData(std::optional<JsonArrayConst>& packData, std::string& logValue, const uint64_t timestamp)
{
    // stop processing here, if no pack data found in message
    // or number of batteries is unkown
    if (!packData.has_value() || _stats->_num_batteries == 0) { return; }

    // get serial number related to index only if all packs given in message
    if ((*packData).size() == _stats->_num_batteries) {
        for (size_t i = 0 ; i < _stats->_num_batteries ; i++) {
            auto serial = Utils::getJsonElement<String>((*packData)[i], ZENDURE_REPORT_PACK_SERIAL);
            if (!serial.has_value()) {
                DTU_LOGW("Missing serial of battery pack in '%s'", logValue.c_str());
                continue;
            }
            if (_stats->addPackData(i+1, *serial) == nullptr) {
                DTU_LOGW("Invalid or unknown serial '%s' in '%s'", (*serial).c_str(), logValue.c_str());
            }
        }
    }

    // check if our array has got inconsistant
    if (_stats->_packData.size() > _stats->_num_batteries) {
        DTU_LOGD("Detected inconsitency of pack data - resetting internal data buffer!");
        _stats->_packData.clear();
        return;
    }

    // get additional data only if all packs were identified
    if (_stats->_packData.size() != _stats->_num_batteries) {
        return;
    }

    for (auto packDataJson : *packData) {
        auto serial = Utils::getJsonElement<String>(packDataJson, ZENDURE_REPORT_PACK_SERIAL);

        // do not waste processing time if nothing to do
        if (!serial.has_value()) { return; }

        processPackDataJson(packDataJson, *serial, timestamp);
    }

    // calculate additional statistics from received data
    calculatePackStats(timestamp);
}

void Provider::calculatePackStats(const uint64_t timestamp)
{
    if (_stats->_num_batteries == 0) { return; }

    std::optional<uint16_t> capacity = std::nullopt;
    std::optional<uint16_t> capacity_avail = std::nullopt;

    size_t avg_count = 0;
    size_t soc_count = 0;
    std::optional<float> soc = std::nullopt;
    std::optional<float> power = std::nullopt;
    std::optional<float> current = std::nullopt;
    std::optional<uint32_t> cellAvg = std::nullopt;
    std::optional<uint16_t> cellMin = std::nullopt;
    std::optional<uint16_t> cellMax = std::nullopt;
    std::optional<float> cellTemp = std::nullopt;

    for (size_t i = 1 ; i <= _stats->_num_batteries ; i++) {
        auto pack = _stats->getPackData(i);
        if (pack == nullptr) { continue; }

        if (pack->_capacity.has_value()) {
            capacity = capacity.value_or(0) + *pack->_capacity;
        }
        if (pack->_capacity_avail.has_value()) {
            capacity_avail = capacity_avail.value_or(0) + *pack->_capacity_avail;
        }

        if (pack->_cell_voltage_avg.has_value()) {
            cellAvg = cellAvg.value_or(0) + *pack->_cell_voltage_avg;
            avg_count++;
        }
        if (pack->_soc_level.has_value()) {
            soc = soc.value_or(0) + *pack->_soc_level;
            soc_count++;
        }
        if (pack->_power.has_value()) {
            power = power.value_or(0) + *pack->_power;
        }
        if (pack->_current.has_value()) {
            current = current.value_or(0) + *pack->_current;
        }
        if (pack->_cell_voltage_min.has_value()) {
            cellMin = std::min(pack->_cell_voltage_min.value_or(std::numeric_limits<uint16_t>::max()), *pack->_cell_voltage_min);
        }
        if (pack->_cell_voltage_max.has_value()) {
            cellMax = std::max(pack->_cell_voltage_max.value_or(std::numeric_limits<uint16_t>::min()), *pack->_cell_voltage_max);
        }
        if (pack->_cell_temperature_max.has_value()) {
            cellTemp = std::max(pack->_cell_temperature_max.value_or(std::numeric_limits<float>::min()), *pack->_cell_temperature_max);
        }
    }

    if (cellMin.has_value() && cellMax.has_value()) {
        _stats->_cellDeltaMilliVolt = *cellMax - *cellMin;
    }
    if (cellAvg.has_value() && avg_count > 0) {
        _stats->_cellAvgMilliVolt = std::round(*cellAvg / avg_count);
    }
    if (soc.has_value() && soc_count > 0) {
        setSoC(*soc / soc_count, timestamp);
    }
    if (current.has_value()) {
        _stats->setCurrent(*current, 1, timestamp);
    }

    _stats->_cellMinMilliVolt = cellMin;
    _stats->_cellMaxMilliVolt = cellMax;
    _stats->_cellTemperature = cellTemp;

    _stats->_capacity = capacity;
    _stats->_capacity_avail = capacity_avail;
}

void Provider::calculateEfficiency()
{
    float in = static_cast<float>(_stats->_input_power.value_or(0));
    float out = static_cast<float>(_stats->_output_power.value_or(0));
    float efficiency = 0.0;

    in += static_cast<float>(_stats->_discharge_power.value_or(0));
    out += static_cast<float>(_stats->_charge_power.value_or(0));

    if (in <= 0.0) {
        _stats->_efficiency.reset();
        return;
    }

    efficiency = out / in;

    if (efficiency > 1.0 || efficiency < 0.0) {
        _stats->_efficiency.reset();
        return;
    }

    _stats->_efficiency = efficiency * 100;
}

void Provider::setSoC(const float soc, const uint32_t timestamp /* = 0 */, const uint8_t precision /* = 2 */)
{
    time_t now;
    auto const& config = Configuration.get();
    auto const chargeThroughState = _stats->_charge_through_state.value_or(ChargeThroughState::Disabled);

    if (Utils::getEpoch(&now)) {
        if (soc >= 100.0) {
            _stats->_last_full_timestamp = now;
            publishPersistentSettings(ZENDURE_PERSISTENT_SETTINGS_LAST_FULL, String(now));

            if (chargeThroughState == ChargeThroughState::Soft || chargeThroughState == ChargeThroughState::Hard) {
                setChargeThroughState(ChargeThroughState::Keep);
            }
        }
        if (soc < static_cast<float>(config.Battery.Zendure.ChargeThroughResetLevel) && chargeThroughState == ChargeThroughState::Keep) {
            setChargeThroughState(ChargeThroughState::Idle);
        }
        if (soc <= 0.0) {
            _stats->_last_empty_timestamp = now;
            publishPersistentSettings(ZENDURE_PERSISTENT_SETTINGS_LAST_EMPTY, String(now));
        }
    }

    _stats->setSoC(soc, precision, timestamp ? timestamp : millis());
}

void Provider::onMqttMessagePersistentSettings(espMqttClientTypes::MessageProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len)
{
    String t(topic);
    String string(reinterpret_cast<const char*>(payload), len);
    auto integer = static_cast<uint64_t>(string.toInt());

    DTU_LOGD("Received Persistent Settings %s = %s [aka %" PRId64 "]", topic, string.substring(0, 32).c_str(), integer);

    if (t.endsWith(ZENDURE_PERSISTENT_SETTINGS_LAST_FULL) && integer) {
        _stats->_last_full_timestamp = integer;
        return;
    }
    if (t.endsWith(ZENDURE_PERSISTENT_SETTINGS_LAST_EMPTY) && integer) {
        _stats->_last_empty_timestamp = integer;
        return;
    }
    if (t.endsWith(ZENDURE_PERSISTENT_SETTINGS_CHARGE_THROUGH)) {
        // only accept state from MQTT if not set during init
        if (_stats->_charge_through_state.has_value()) { return; }

        auto mode = Stats::chargeThroughStateFromString(string);
        if (mode.has_value()) {
            // if we recieve disabled state but configuration is set to enabled, overwrite with IDLE state
            if (Configuration.get().Battery.Zendure.ChargeThroughEnable && mode == ChargeThroughState::Disabled) {
                mode = ChargeThroughState::Idle;
            }

            // interpret recent values if decodeable
            setChargeThroughState(*mode, false);
        } else {
            // otherwise, interpret legacy values that might be stored on broker
            setChargeThroughState(integer > 0 ? ChargeThroughState::Hard : ChargeThroughState::Idle, false);
        }

        return;
    }
}

void Provider::publishPersistentSettings(const char* subtopic, const String& payload)
{
    if (!_topicPersistentSettings.isEmpty())
    {
        DTU_LOGD("Writing Persistent Settings %s = %s\r\n",
                String(_topicPersistentSettings + subtopic).c_str(),
                payload.substring(0, 32).c_str());
        MqttSettings.publishGeneric(_topicPersistentSettings + subtopic, payload, true);
    }
}


} // namespace Batteries::Zendure
