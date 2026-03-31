#include <functional>
#include <Configuration.h>
#include <battery/zendure/LocalMqttProvider.h>
#include <MqttSettings.h>
#include <SunPosition.h>
#include <Utils.h>
#include <LogHelper.h>

#undef TAG
static const char* TAG = "battery";
static const char* SUBTAG = "Zendure";

namespace Batteries::Zendure {

LocalMqttProvider::LocalMqttProvider()
    : Provider() { }

bool LocalMqttProvider::init()
{
    auto const& config = Configuration.get();

    if (!Provider::init()) { return false; }

    String deviceType = String();

    DTU_LOGD("Settings %" PRIu32, config.Battery.Zendure.DeviceType);
    {
        String deviceName = String();
        switch (config.Battery.Zendure.DeviceType) {
            case 0:
                deviceType = ZENDURE_HUB1200;
                deviceName = ZENDURE_HUB1200_NAME;
                _full_log_supported = true;
                break;
            case 1:
                deviceType = ZENDURE_HUB2000;
                deviceName = ZENDURE_HUB2000_NAME;
                _full_log_supported = true;
                break;
            case 2:
                deviceType = ZENDURE_AIO2400;
                deviceName = ZENDURE_AIO2400_NAME;
                break;
            case 3:
                deviceType = ZENDURE_ACE1500;
                deviceName = ZENDURE_ACE1500_NAME;
                break;
            case 4:
                deviceType = ZENDURE_HYPER2000_A;
                deviceName = ZENDURE_HYPER2000_NAME;
                break;
            case 5:
                deviceType = ZENDURE_HYPER2000_B;
                deviceName = ZENDURE_HYPER2000_NAME;
                break;
            default:
                DTU_LOGE("Invalid device type!");
                return false;
        }

        // setup static device info
        DTU_LOGI("Device name '%s' - LOG messages are %s supported\r\n",
                deviceName.c_str(), (_full_log_supported ? "fully" : "partly"));
        _stats->setDevice(std::move(deviceName));
    }

    // store device ID as we will need them for checking when receiving messages
    setTopics(deviceType, config.Battery.Zendure.DeviceId);

    // subscribe for log messages
    MqttSettings.subscribe(_topicLog, 0/*QoS*/,
            std::bind(&LocalMqttProvider::onMqttMessageLog,
                this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4)
            );
    DTU_LOGD("Subscribed to '%s' for status readings", _topicLog.c_str());

    // subscribe for report messages
    MqttSettings.subscribe(_topicReport, 0/*QoS*/,
            std::bind(&LocalMqttProvider::onMqttMessageReport,
                this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4)
            );
    DTU_LOGD("Subscribed to '%s' for status readings", _topicReport.c_str());

    // subscribe for timesync messages
    MqttSettings.subscribe(_topicTimesync, 0/*QoS*/,
            std::bind(&LocalMqttProvider::onMqttMessageTimesync,
                this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4)
            );
    DTU_LOGD("Subscribed to '%s' for timesync requests", _topicTimesync.c_str());

    // pre-generate the full update request
    JsonDocument root;
    JsonArray array = root[ZENDURE_REPORT_PROPERTIES].to<JsonArray>();
    array.add("getAll");
    array.add("getInfo");
    serializeJson(root, _payloadFullUpdate);

    // disable charge through cycle if disable by config
    if (!config.Battery.Zendure.ChargeThroughEnable) {
        setChargeThroughState(ChargeThroughState::Disabled);
    }

    // check if we are allowed to write stuff
    if (config.Battery.Zendure.ControlMode == BatteryZendureConfig::ControlMode::ControlModeReadOnly) {
        DTU_LOGI("Running in READ-ONLY mode");

        // forget about write topic and payload to prevent it will ever be written
        _topicWrite.clear();
    }

    DTU_LOGI("INIT DONE");
    return true;
}

void LocalMqttProvider::setTopics(const String& deviceType, const String& deviceId) {
    String baseTopic = "/" + deviceType + "/" + deviceId + "/";

    _topicRead   = "iot" + baseTopic + "properties/read";
    _topicWrite  = "iot" + baseTopic + "properties/write";
    _topicLog    = baseTopic + "log";
    _topicReport = baseTopic + "properties/report";

    _topicTimesync      = baseTopic + "time-sync";
    _topicTimesyncReply = "iot" + baseTopic + "time-sync/reply";
}

void LocalMqttProvider::deinit()
{
    Provider::deinit();

    if (!_topicReport.isEmpty()) {
        MqttSettings.unsubscribe(_topicReport);
        _topicReport.clear();
    }
    if (!_topicLog.isEmpty()) {
        MqttSettings.unsubscribe(_topicLog);
        _topicLog.clear();
    }
    if (!_topicTimesync.isEmpty()) {
        MqttSettings.unsubscribe(_topicTimesync);
        _topicTimesync.clear();
    }
}

void LocalMqttProvider::writeSettings() {
    if (_topicWrite.isEmpty()) {
        return;
    }

    auto const& config = Configuration.get();

    setBuzzer(config.Battery.Zendure.BuzzerEnable);
    setAutoshutdown(config.Battery.Zendure.AutoShutdown);
    //setBypassMode(config.Battery.Zendure.BypassMode);

    publishProperty(_topicWrite, ZENDURE_REPORT_PV_BRAND, "1");         // means Hoymiles
    publishProperty(_topicWrite, ZENDURE_REPORT_PV_AUTO_MODEL, "0");    // we did static setup
    publishProperty(_topicWrite, ZENDURE_REPORT_SMART_MODE, "0");       // disable smart mode

    // publishProperties(_topicWrite,
    //     ZENDURE_REPORT_PV_BRAND,        "1",    // means Hoymiles
    //     ZENDURE_REPORT_PV_AUTO_MODEL,   "0",    // we did static setup
    //     ZENDURE_REPORT_SMART_MODE,      "0"     // disable smart mode
    // );

    // if running in OnlyOnce mode, forget about write topic to prevent it will ever be written again
    if (config.Battery.Zendure.ControlMode == BatteryZendureConfig::ControlMode::ControlModeOnce) {
        _topicWrite.clear();
    }
}

void LocalMqttProvider::setBuzzer(bool enable) const
{
    if (_topicWrite.isEmpty() || !alive() || _stats->_buzzer == enable) {
        return;
    }

    publishProperty(_topicWrite, ZENDURE_REPORT_BUZZER_SWITCH, String(enable ? "1" : "0"));
    DTU_LOGD("%s buzzer", enable ? "Enabling" : "Disabling");
}

void LocalMqttProvider::setAutoshutdown(bool enable) const
{
    if (_topicWrite.isEmpty() || !alive() || _stats->_auto_shutdown == enable) {
        return;
    }

    publishProperty(_topicWrite, ZENDURE_REPORT_AUTO_SHUTDOWN, String(enable ? "1" : "0"));
    DTU_LOGD("%s autoshutdown", enable ? "Enabling" : "Disabling");
}

void LocalMqttProvider::timesync()
{
    time_t now;
    if (!_topicTimesyncReply.isEmpty() && Utils::getEpoch(&now)) {
        MqttSettings.publishGeneric(_topicTimesyncReply, "{\"zoneOffset\": \"+00:00\", \"messageId\": " + String(++_messageCounter) + ", \"timestamp\": " + String(now) + "}", false, 0);
        DTU_LOGD("Timesync Reply Sent");
    }
}

void LocalMqttProvider::onMqttMessageTimesync(espMqttClientTypes::MessageProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len)
{
    DTU_LOGD("Timesync Request Received");
    timesync();
}

void LocalMqttProvider::onMqttMessageReport(espMqttClientTypes::MessageProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len)
{
    auto ms = millis();

    std::string const src = std::string(reinterpret_cast<const char*>(payload), len);
    std::string logValue = src.substr(0, 64);
    if (src.length() > logValue.length()) { logValue += "..."; }

    JsonDocument json;

    const DeserializationError error = deserializeJson(json, src);
    if (error) {
        DTU_LOGE("cannot parse payload '%s' as JSON", logValue.c_str());
        return;
    }

    if (json.overflowed()) {
        DTU_LOGE("payload too large to process as JSON");
        return;
    }

    auto obj = json.as<JsonObjectConst>();

    // validate input data
    // messageId has to be set to "123"
    // deviceId has to be set to the configured deviceId
    if (!json["messageId"].as<String>().equals("123")) {
        DTU_LOGE("Invalid or missing 'messageId' in '%s'", logValue.c_str());
        return;
    }
    if (!json["deviceId"].as<String>().equals(Configuration.get().Battery.Zendure.DeviceId)) {
        DTU_LOGE("Invalid or missing 'deviceId' in '%s'", logValue.c_str());
        return;
    }

    auto props = Utils::getJsonElement<JsonObjectConst>(obj, ZENDURE_REPORT_PROPERTIES, 1);
    auto packData = Utils::getJsonElement<JsonArrayConst>(obj, ZENDURE_REPORT_PACK_DATA, 2);

    processProperties(props, ms);
    processPackData(packData, logValue, ms);
}

void LocalMqttProvider::onMqttMessageLog(espMqttClientTypes::MessageProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len)
{
    auto ms = millis();

    DTU_LOGD("Logging Frame received!");

    std::string const src = std::string(reinterpret_cast<const char*>(payload), len);
    std::string logValue = src.substr(0, 64);
    if (src.length() > logValue.length()) { logValue += "..."; }

    JsonDocument json;

    const DeserializationError error = deserializeJson(json, src);
    if (error) {
        DTU_LOGE("cannot parse payload '%s' as JSON", logValue.c_str());
        return;
    }

    if (json.overflowed()) {
        DTU_LOGE("payload too large to process as JSON");
        return;
    }

    auto obj = json.as<JsonObjectConst>();

    // validate input data
    // deviceId has to be set to the configured deviceId
    // logType has to be set to "2"
    if (!json["deviceId"].as<String>().equals(Configuration.get().Battery.Zendure.DeviceId)) {
        DTU_LOGE("Invalid or missing 'deviceId' in '%s'", logValue.c_str());
        return;
    }
    if (!json["logType"].as<String>().equals("2")) {
        DTU_LOGE("Invalid or missing 'v' in '%s'", logValue.c_str());
        return;
    }

    auto data = Utils::getJsonElement<JsonObjectConst>(obj, ZENDURE_LOG_ROOT, 2);
    if (!data.has_value()) {
        DTU_LOGE("Unable to find 'log' in '%s'", logValue.c_str());
        return;
    }

    _stats->setSerial(Utils::getJsonElement<String>(*data, ZENDURE_LOG_SERIAL));

    auto params = Utils::getJsonElement<JsonArrayConst>(*data, ZENDURE_LOG_PARAMS, 1);
    if (!params.has_value()) {
        DTU_LOGE("Unable to find 'params' in '%s'", logValue.c_str());
        return;
    }

    auto v = *params;

    uint8_t num = v[ZENDURE_LOG_OFFSET_PACKNUM].as<uint8_t>();
    if (num > 0 && num <= ZENDURE_MAX_PACKS) {
        for (size_t i = 1 ; i <= num ; i++) {
            auto pack = _stats->getPackData(i);
            if (pack == nullptr) { continue; }

            pack->setSoCPromille(v[ZENDURE_LOG_OFFSET_PACK_SOC(i)].as<uint16_t>());
            pack->setCellTemperatureMaxCelsius(v[ZENDURE_LOG_OFFSET_PACK_TEMPERATURE(i)].as<int32_t>());
            pack->setCellVoltageMin(v[ZENDURE_LOG_OFFSET_PACK_CELL_MIN(i)].as<uint32_t>());
            pack->setCellVoltageMax(v[ZENDURE_LOG_OFFSET_PACK_CELL_MAX(i)].as<uint32_t>());
            pack->setVoltage(v[ZENDURE_LOG_OFFSET_PACK_VOLTAGE(i)].as<uint16_t>());
            pack->setCurrent(v[ZENDURE_LOG_OFFSET_PACK_CURRENT(i)].as<int16_t>());
            pack->setPower();

            pack->_lastUpdate = ms;
        }
    }
    _stats->_num_batteries = num;

    calculatePackStats(ms);

    // some devices have different log structure - only process for devices explicitly enabled!
    if (_full_log_supported) {
        _stats->setVoltage(v[ZENDURE_LOG_OFFSET_VOLTAGE].as<float>() / 10.0, ms);

        _stats->setAutoRecover(v[ZENDURE_LOG_OFFSET_AUTO_RECOVER].as<uint8_t>());
        _stats->setSocMin(v[ZENDURE_LOG_OFFSET_MIN_SOC].as<float>());

        _stats->setOutputLimit(static_cast<uint16_t>(v[ZENDURE_LOG_OFFSET_OUTPUT_POWER_LIMIT].as<uint32_t>() / 100));
        _stats->setOutputPower(v[ZENDURE_LOG_OFFSET_OUTPUT_POWER].as<uint16_t>());
        _stats->setChargePower(v[ZENDURE_LOG_OFFSET_CHARGE_POWER].as<uint16_t>());
        _stats->setDischargePower(v[ZENDURE_LOG_OFFSET_DISCHARGE_POWER].as<uint16_t>());
        _stats->setSolarPower1(v[ZENDURE_LOG_OFFSET_SOLAR_POWER_MPPT_1].as<uint16_t>());
        _stats->setSolarPower2(v[ZENDURE_LOG_OFFSET_SOLAR_POWER_MPPT_2].as<uint16_t>());
    }

    _stats->_lastUpdate = ms;
    calculateEfficiency();
}

void LocalMqttProvider::shutdown() const
{
    if (!_topicWrite.isEmpty()) {
        publishProperty(_topicWrite, ZENDURE_REPORT_MASTER_SWITCH, "1");
        DTU_LOGD("Shutting down HUB");
    }
}

void LocalMqttProvider::processPackDataJson(JsonVariantConst& packDataJson, const String& serial, const uint64_t timestamp)
{
    auto state = Utils::getJsonElement<uint8_t>(packDataJson, ZENDURE_REPORT_PACK_STATE);
    auto version = Utils::getJsonElement<uint32_t>(packDataJson, ZENDURE_REPORT_PACK_FW_VERSION);
    auto soh = Utils::getJsonElement<uint16_t>(packDataJson, ZENDURE_REPORT_PACK_HEALTH);
    auto voltage = Utils::getJsonElement<uint16_t>(packDataJson, ZENDURE_REPORT_PACK_TOTAL_VOLTAGE);

    // do not waste processing time if nothing to do
    if (!(state.has_value() || version.has_value() || soh.has_value() || voltage.has_value())) {
        return;
    }

    // find pack data related to serial number
    for (auto& entry : _stats->_packData) {
        auto pack = entry.second;
        if (pack->_serial != serial) {
            continue;
        }

        pack->setState(state);
        pack->setSoH(soh);
        pack->setFirmwareVersion(version);

        // Fallback to voltage reported by the FIRST pack if we are unable to use values from loggin messages.
        // This is only precise, if there is exactly ONE pack - when there are more packs, using only the first
        // is not sufficient and voltage will be completely unavailable to prevent erroneous reporting.
        if (voltage.has_value() && !_full_log_supported && entry.first == 1 && _stats->_num_batteries == 1) {
            _stats->setVoltage(static_cast<float>(*voltage) / 100.0, timestamp);
        }

        pack->_lastUpdate = timestamp;

        // found the pack we searched for, so terminate loop here
        break;
    }
}

void LocalMqttProvider::setBypassMode(BatteryZendureConfig::BypassMode_t mode) const
{
    if (_topicWrite.isEmpty() || !alive()) {
        return;
    }

    if (_stats->_bypass_mode != mode) {
        publishProperty(_topicWrite, ZENDURE_REPORT_BYPASS_MODE, String(static_cast<uint8_t>(mode)));
        DTU_LOGD("Adjusting bypassmode from %" PRIu8 " to %" PRIu8 "", _stats->_bypass_mode.value_or(BatteryZendureConfig::BypassMode_t::Automatic), mode);
    }

    bool recover = (mode == BatteryZendureConfig::BypassMode_t::Automatic);
    if (_stats->_auto_recover != recover) {
        publishProperty(_topicWrite, ZENDURE_REPORT_AUTO_RECOVER, String(static_cast<uint8_t>(recover)));
        DTU_LOGD("Adjusting autorecover from %" PRIu8 " to %" PRIu8 "", _stats->_auto_recover.value_or(false), recover);
    }
}

} // namespace Batteries::Zendure
