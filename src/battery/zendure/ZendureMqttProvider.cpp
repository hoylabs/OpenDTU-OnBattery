#include <functional>
#include <Configuration.h>
#include <battery/zendure/ZendureMqttProvider.h>
#include <MqttSettings.h>
#include <SunPosition.h>
#include <Utils.h>
#include <LogHelper.h>

#undef TAG
static const char* TAG = "battery";
static const char* SUBTAG = "Zendure";

namespace Batteries::Zendure {

ZendureMqttProvider::ZendureMqttProvider()
    : Provider() { }

bool ZendureMqttProvider::init()
{
    auto const& config = Configuration.get();

    if (strlen(config.Battery.Zendure.AppKey) != 8) {
        DTU_LOGE("Invalid app key '%s'!", config.Battery.Zendure.AppKey);
        return false;
    }

    if (strlen(config.Battery.Zendure.Secret) != 32) {
        DTU_LOGE("Invalid secret '%s'!", config.Battery.Zendure.Secret);
        return false;
    }

    if (strlen(config.Battery.Zendure.Server) < 4) {
        DTU_LOGE("Invalid server '%s'!", config.Battery.Zendure.Server);
        return false;
    }

    if (config.Battery.Zendure.Port < 1) {
        DTU_LOGE("Invalid port '%" PRIu16 "'!", config.Battery.Zendure.Port);
        return false;
    }

    if (strlen(config.Battery.Zendure.ClientId) < 2) {
        DTU_LOGE("Invalid client id '%s'!", config.Battery.Zendure.ClientId);
        return false;
    }

    if (!Provider::init()) { return false; }

    // store device ID as we will need them for checking when receiving messages
    setTopics(config.Battery.Zendure.AppKey, config.Battery.Zendure.DeviceId);

    // disable charge through cycle if disable by config
    setChargeThroughState(ChargeThroughState::Disabled);

    DTU_LOGI("INIT CLOUD CONNECTION");

    using std::placeholders::_1;
    NetworkSettings.onEvent(std::bind(&ZendureMqttProvider::NetworkEvent, this, _1));
    createMqttClientObject();
    performConnect();

    DTU_LOGI("INIT DONE");
    return true;
}

void ZendureMqttProvider::setTopics(const String& deviceType, const String& deviceId) {
    String baseTopic = deviceType + "/" + deviceId + "/";
    _topicReport = baseTopic + "state";
}

void ZendureMqttProvider::deinit()
{
    Provider::deinit();

    if (!_topicReport.isEmpty()) {
        _topicReport.clear();
    }

    if (_mqttClient != nullptr) {
        _mqttClient->disconnect(true);
        delete _mqttClient;
        _mqttClient = nullptr;
    }
}

void ZendureMqttProvider::onMqttMessageReport(espMqttClientTypes::MessageProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len)
{
    if (!_topicReport.equals(topic)) { return; }

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

    DTU_LOGD("ZendureCloud: %s", logValue.c_str());

    std::optional<JsonObjectConst> props = obj;
    auto packData = Utils::getJsonElement<JsonArrayConst>(obj, ZENDURE_REPORT_PACK_DATA, 2);

    processProperties(props, ms);

    processPackData(packData, logValue, ms);

    // this seems to be NOT reliable when useing the offical API...
    //calculateEfficiency();
}

void ZendureMqttProvider::processPackData(std::optional<JsonArrayConst>& packData, std::string& logValue, const uint64_t timestamp)
{
    // stop processing here, if no pack data found in message
    if (!packData.has_value()) { return; }

    // try to detetct number of packs from packData
    auto packSize = (*packData).size();

    // stop processing here, if no pack data found in message
    if (packSize == 0) { return; }

    if (_stats->_num_batteries < packSize) {
        uint8_t num_batteries = 0;
        uint8_t dummies = 0;

        DTU_LOGD("Battery count unkown - try to guess from payload containing %" PRId32 " entries", packSize);

        // Zendure delivers dummy pack data containing only serial number from time to time
        // this can be used to estimate number of packs - but sometimes, there is payload added, too...
        for (size_t i = 0 ; i < packSize ; i++) {
            auto items = (*packData)[i].size();
            auto serial = Utils::getJsonElement<String>((*packData)[i], ZENDURE_REPORT_PACK_SERIAL);

            // if there is no serial number, we cannot use this pack data
            if (!serial.has_value()) {
                DTU_LOGD("Guessing failed (serial=%s, items=%" PRId32 ")", serial.has_value() ? "OK" : "MISSING", items);
                break;
            }

            // a valid dummy entry just contains the serial number and nothing else
            if (items == 1) {
                dummies++;
            }

            num_batteries++;
        }

        // only use result if we are likely to be right...
        // - all entries of the list were used
        // - at least one dummy entry has to be in place
        if (dummies > 0 && num_batteries == packSize) {
            DTU_LOGI("Detected number of packs from dummy pack data messages. We have %" PRIu8 " packs.", num_batteries);
            _stats->_num_batteries = num_batteries;
        }
    }

    Provider::processPackData(packData, logValue, timestamp);
}

void ZendureMqttProvider::processProperties(std::optional<JsonObjectConst>& props, const uint64_t timestamp)
{
    if (!props.has_value()) { return; }

    Provider::processProperties(props, timestamp);

    _stats->setSerial(Utils::getJsonElement<String>(*props, ZENDURE_REPORT_SERIAL));

    auto num_batteries = Utils::getJsonElement<uint8_t>(*props, ZENDURE_REPORT_PACK_NUM);
    if (num_batteries.has_value()) {
        _stats->_num_batteries = *num_batteries;
    }

    _stats->updateSolarInputPower(Utils::getJsonElement<uint16_t>(*props, ZENDURE_REPORT_SOLAR_INPUT_POWER));
}

void ZendureMqttProvider::processPackDataJson(JsonVariantConst& packDataJson, const String& serial, const uint64_t timestamp)
{
    // find pack data related to serial number
    for (auto& entry : _stats->_packData) {
        auto pack = entry.second;
        if (pack->_serial != serial) { continue; }

        pack->setState(Utils::getJsonElement<uint8_t>(packDataJson, ZENDURE_REPORT_PACK_STATE));
        pack->setFirmwareVersion(Utils::getJsonElement<uint32_t>(packDataJson, ZENDURE_REPORT_PACK_FW_VERSION));

        pack->setVoltage(Utils::getJsonElement<uint16_t>(packDataJson, ZENDURE_REPORT_PACK_TOTAL_VOLTAGE));
        pack->setPower(Utils::getJsonElement<uint16_t>(packDataJson, ZENDURE_REPORT_PACK_POWER));
        pack->setCurrent();

        pack->setSoCPercent(Utils::getJsonElement<uint8_t>(packDataJson, ZENDURE_REPORT_PACK_SOC));
        pack->setCellTemperatureMaxKelvin(Utils::getJsonElement<float>(packDataJson, ZENDURE_REPORT_PACK_CELL_MAX_TEMPERATURE));
        pack->setCellVoltageMin(Utils::getJsonElement<uint16_t>(packDataJson, ZENDURE_REPORT_PACK_CELL_MIN_VOLTAGE));
        pack->setCellVoltageMax(Utils::getJsonElement<uint16_t>(packDataJson, ZENDURE_REPORT_PACK_CELL_MAX_VOLTAGE));

        pack->_lastUpdate = timestamp;

        // found the pack we searched for - stop searching
        return;
    }
}

void ZendureMqttProvider::performConnect()
{
    if (!NetworkSettings.isConnected()) { return; }

    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    std::lock_guard<std::mutex> lock(_clientLock);
    if (_mqttClient == nullptr) {
        return;
    }

    ESP_LOGI(TAG, "Connecting to Zendure MQTT...");
    const CONFIG_T& config = Configuration.get();

    static_cast<espMqttClient*>(_mqttClient)->setServer(config.Battery.Zendure.Server, config.Battery.Zendure.Port);
    static_cast<espMqttClient*>(_mqttClient)->setCredentials(config.Battery.Zendure.AppKey, config.Battery.Zendure.Secret);
    static_cast<espMqttClient*>(_mqttClient)->setClientId(config.Battery.Zendure.ClientId);
    static_cast<espMqttClient*>(_mqttClient)->setCleanSession(false);
    static_cast<espMqttClient*>(_mqttClient)->onConnect(std::bind(&ZendureMqttProvider::onMqttConnect, this, _1));
    static_cast<espMqttClient*>(_mqttClient)->onDisconnect(std::bind(&ZendureMqttProvider::onMqttDisconnect, this, _1));
    static_cast<espMqttClient*>(_mqttClient)->onMessage(std::bind(&ZendureMqttProvider::onMqttMessage, this, _1, _2, _3, _4, _5, _6));

    _mqttClient->connect();
}

void ZendureMqttProvider::performDisconnect()
{
    std::lock_guard<std::mutex> lock(_clientLock);
    if (_mqttClient == nullptr) {
        return;
    }
    _mqttClient->disconnect();
}

void ZendureMqttProvider::performReconnect()
{
    performDisconnect();

    createMqttClientObject();

    _mqttReconnectTimer.once(
        2, +[](ZendureMqttProvider* instance) { instance->performConnect(); }, this);
}

bool ZendureMqttProvider::getConnected()
{
    std::lock_guard<std::mutex> lock(_clientLock);
    if (_mqttClient == nullptr) {
        return false;
    }
    return _mqttClient->connected();
}

void ZendureMqttProvider::createMqttClientObject()
{
    std::lock_guard<std::mutex> lock(_clientLock);
    if (_mqttClient != nullptr) {
        delete _mqttClient;
        _mqttClient = nullptr;
    }

    _mqttClient = static_cast<MqttClient*>(new espMqttClient);
}

void ZendureMqttProvider::onMqttConnect(const bool sessionPresent)
{
    ESP_LOGI(TAG, "Connected to Zendure MQTT.");

    std::lock_guard<std::mutex> lock(_clientLock);
    if (_mqttClient != nullptr) {
        _mqttClient->subscribe(_topicReport.c_str(), 0);
    }
}

void ZendureMqttProvider::onMqttDisconnect(espMqttClientTypes::DisconnectReason reason)
{
    static constexpr frozen::map<espMqttClientTypes::DisconnectReason, frozen::string, 8> reasons = {
        { espMqttClientTypes::DisconnectReason::USER_OK, "USER_OK" },
        { espMqttClientTypes::DisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION, "MQTT_UNACCEPTABLE_PROTOCOL_VERSION" },
        { espMqttClientTypes::DisconnectReason::MQTT_IDENTIFIER_REJECTED, "MQTT_IDENTIFIER_REJECTED" },
        { espMqttClientTypes::DisconnectReason::MQTT_SERVER_UNAVAILABLE, "MQTT_SERVER_UNAVAILABLE" },
        { espMqttClientTypes::DisconnectReason::MQTT_MALFORMED_CREDENTIALS, "MQTT_MALFORMED_CREDENTIALS" },
        { espMqttClientTypes::DisconnectReason::MQTT_NOT_AUTHORIZED, "MQTT_NOT_AUTHORIZED" },
        { espMqttClientTypes::DisconnectReason::TLS_BAD_FINGERPRINT, "TLS_BAD_FINGERPRINT" },
        { espMqttClientTypes::DisconnectReason::TCP_DISCONNECTED, "TCP_DISCONNECTED" },
    };

    auto it = reasons.find(reason);
    const char* reasonStr = (it != reasons.end()) ? it->second.data() : "Unknown";

    ESP_LOGW(TAG, "Disconnected from Zendure MQTT. Reason: %s", reasonStr);

    _mqttReconnectTimer.once(
        2, +[](ZendureMqttProvider* instance) { instance->performConnect(); }, this);
}

void ZendureMqttProvider::onMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, const size_t len, const size_t index, const size_t total)
{
    ESP_LOGD(TAG, "Received Zendure MQTT message on topic '%s' (Bytes %zu-%zu/%zu)",
        topic, index + 1, (index + len), total);

     // shortcut for most MQTT messages, which are not fragmented
    if (index == 0 && len == total) {
        return onMqttMessageReport(properties, topic, payload, len);
    }

    auto& fragment = _fragments[String(topic)];

    // first fragment of a new message
    if (index == 0) {
        fragment.clear();
        fragment.reserve(total);
    }

    fragment.insert(fragment.end(), payload, payload + len);

    if (fragment.size() < total) {
        return;
    } // wait for last fragment

    ESP_LOGD(TAG, "Fragmented MQTT message reassembled for topic '%s'", topic);

    onMqttMessageReport(properties, topic, payload, total);

    _fragments.erase(String(topic));
}

void ZendureMqttProvider::NetworkEvent(network_event event)
{
    switch (event) {
    case network_event::NETWORK_GOT_IP:
        ESP_LOGD(TAG, "Network connected");
        performConnect();
        break;
    case network_event::NETWORK_DISCONNECTED:
        ESP_LOGD(TAG, "Network lost connection");
        _mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
        break;
    default:
        break;
    }
}

} // namespace Batteries::Zendure
