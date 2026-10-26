// SPDX-License-Identifier: GPL-2.0-or-later
#include <battery/CanProvider.h>
#include <PinMapping.h>
#include <driver/twai.h>
#include <LogHelper.h>
#include <algorithm>
#include <cstring>

#undef TAG
static const char* TAG = "battery";
#define SUBTAG _providerName

namespace Batteries {

bool CanProvider::init(char const* providerName)
{
    _providerName = providerName;

    DTU_LOGI("Initialize interface...");

    const PinMapping_t& pin = PinMapping.get();
    DTU_LOGD("Interface rx = %d, tx = %d", pin.battery_rx, pin.battery_tx);

    if (pin.battery_rx <= GPIO_NUM_NC || pin.battery_tx <= GPIO_NUM_NC) {
        DTU_LOGE("Invalid pin config");
        return false;
    }

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(pin.battery_tx, pin.battery_rx, TWAI_MODE_NORMAL);

    // interrupts at level 1 are in high demand, at least on ESP32-S3 boards,
    // but only a limited amount can be allocated. failing to allocate an
    // interrupt in the TWAI driver will cause a bootloop. we therefore
    // register the TWAI driver's interrupt at level 2. level 2 interrupts
    // should be available -- we don't really know. we would love to have the
    // esp_intr_dump() function, but that's not available yet in our version
    // of the underlying esp-idf.
    g_config.intr_flags = ESP_INTR_FLAG_LEVEL2;

    // batteries send their frames in bursts (and answer requests on top of
    // that), the default of 5 frames overflows easily
    g_config.rx_queue_len = 32;

    // Initialize configuration structures using macro initializers
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Install TWAI driver
    esp_err_t twaiLastResult = twai_driver_install(&g_config, &t_config, &f_config);
    switch (twaiLastResult) {
        case ESP_OK:
            DTU_LOGI("TWAI driver installed");
            break;
        case ESP_ERR_INVALID_ARG:
            DTU_LOGE("TWAI driver install - invalid arg");
            return false;
            break;
        case ESP_ERR_NO_MEM:
            DTU_LOGE("TWAI driver install - no memory");
            return false;
            break;
        case ESP_ERR_INVALID_STATE:
            DTU_LOGE("TWAI driver install - invalid state");
            return false;
            break;
    }

    // Start TWAI driver
    twaiLastResult = twai_start();
    switch (twaiLastResult) {
        case ESP_OK:
            DTU_LOGI("TWAI driver started");
            break;
        case ESP_ERR_INVALID_STATE:
            DTU_LOGE("TWAI driver start - invalid state");
            return false;
            break;
    }

    return true;
}

void CanProvider::deinit()
{
    // Stop TWAI driver
    esp_err_t twaiLastResult = twai_stop();
    switch (twaiLastResult) {
        case ESP_OK:
            DTU_LOGI("TWAI driver stopped");
            break;
        case ESP_ERR_INVALID_STATE:
            DTU_LOGE("TWAI driver stop - invalid state");
            break;
    }

    // Uninstall TWAI driver
    twaiLastResult = twai_driver_uninstall();
    switch (twaiLastResult) {
        case ESP_OK:
            DTU_LOGI("TWAI driver uninstalled");
            break;
        case ESP_ERR_INVALID_STATE:
            DTU_LOGE("TWAI driver uninstall - invalid state");
            break;
    }
}

void CanProvider::loop()
{
    onLoop();

    // Check for messages. twai_receive is blocking when there is no data so we return if there are no frames in the buffer
    twai_status_info_t status_info;
    esp_err_t twaiLastResult = twai_get_status_info(&status_info);
    if (twaiLastResult != ESP_OK) {
        switch (twaiLastResult) {
            case ESP_ERR_INVALID_ARG:
                DTU_LOGE("TWAI driver get status - invalid arg");
                break;
            case ESP_ERR_INVALID_STATE:
                DTU_LOGE("TWAI driver get status - invalid state");
                break;
        }
        return;
    }

    // only possible while transmitting: transmit errors (e.g. while the
    // battery is off) can put the controller into bus off state, in which it
    // does not receive either
    if (status_info.state == TWAI_STATE_BUS_OFF) {
        DTU_LOGW("TWAI bus off, initiating recovery");
        twai_initiate_recovery();
        return;
    }
    if (status_info.state == TWAI_STATE_STOPPED) {
        twai_start(); // after a recovery
        return;
    }

    if (status_info.msgs_to_rx == 0) {
        return;
    }

    // Wait for message to be received, function is blocking
    twai_message_t rx_message;
    if (twai_receive(&rx_message, pdMS_TO_TICKS(100)) != ESP_OK) {
        DTU_LOGE("Failed to receive message");
        return;
    }

    DTU_LOGD("Received CAN message: 0x%04X (%d bytes)",
            rx_message.identifier, rx_message.data_length_code);
    LogHelper::dumpBytes(TAG, _providerName, rx_message.data, rx_message.data_length_code);

    onMessage(rx_message);
}

bool CanProvider::transmit(uint32_t identifier, uint8_t const* data, uint8_t length, bool extended)
{
    // only drive the bus when the controller is healthy and no transmission
    // is still pending. a stuck (unacknowledged) frame must not pile up and
    // push the controller towards bus off, which would also stop reception.
    twai_status_info_t status;
    if (twai_get_status_info(&status) != ESP_OK
            || status.state != TWAI_STATE_RUNNING
            || status.msgs_to_tx > 0) {
        DTU_LOGW("TX 0x%X skipped (state=%d msgs_to_tx=%d)",
                (unsigned)identifier, (int)status.state, (int)status.msgs_to_tx);
        return false;
    }

    twai_message_t message = {};
    message.identifier = identifier;
    message.extd = extended ? 1 : 0;
    // keep automatic retransmission on (like the working Huawei TWAI code):
    // single-shot frames silently lose arbitration against the battery's
    // continuous frame bursts and never reach it. the msgs_to_tx guard above
    // keeps only one frame in flight, and bus off recovery in loop() handles
    // the battery-absent case.
    message.data_length_code = std::min<uint8_t>(length, TWAI_FRAME_MAX_DLC);
    memcpy(message.data, data, message.data_length_code);

    if (twai_transmit(&message, 0) != ESP_OK) {
        DTU_LOGD("Failed to queue CAN message 0x%04X", identifier);
        return false;
    }

    DTU_LOGD("Sent CAN message: 0x%04X (%d bytes)", identifier, message.data_length_code);
    LogHelper::dumpBytes(TAG, _providerName, message.data, message.data_length_code);
    return true;
}

uint8_t CanProvider::readUnsignedInt8(uint8_t *data)
{
    return data[0];
}

uint16_t CanProvider::readUnsignedInt16(uint8_t *data)
{
    return (data[1] << 8) | data[0];
}

int16_t CanProvider::readSignedInt16(uint8_t *data)
{
    return this->readUnsignedInt16(data);
}

int32_t CanProvider::readSignedInt24(uint8_t *data)
{
    return (data[2] << 16) | (data[1] << 8) | data[0];
}

uint32_t CanProvider::readUnsignedInt32(uint8_t *data)
{
    return (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
}

float CanProvider::scaleValue(int32_t value, float factor)
{
    return value * factor;
}

bool CanProvider::getBit(uint32_t value, uint8_t bit)
{
    return (value >> bit) & 1;
}

} // namespace Batteries
