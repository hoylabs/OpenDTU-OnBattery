// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Malte Schmidt and others
 */
#include <gridcharger/huawei/TWAI.h>
#include "MessageOutput.h"
#include "PinMapping.h"
#include "Configuration.h"
#include <driver/twai.h>

namespace GridCharger::Huawei {

TWAI::~TWAI()
{
    stopLoop();

    if (twai_stop() != ESP_OK) {
        MessageOutput.print("[Huawei::TWAI] failed to stop driver\r\n");
        return;
    }

    if (twai_driver_uninstall() != ESP_OK) {
        MessageOutput.print("[Huawei::TWAI] failed to uninstall driver\r\n");
    }

    MessageOutput.print("[Huawei::TWAI] driver stopped and uninstalled\r\n");
}

bool TWAI::init()
{
    const PinMapping_t& pin = PinMapping.get();

    MessageOutput.printf("[Huawei::TWAI] rx = %d, tx = %d\r\n",
            pin.huawei_rx, pin.huawei_tx);

    if (pin.huawei_rx < 0 || pin.huawei_tx < 0) {
        MessageOutput.print("[Huawei::TWAI] invalid pin config\r\n");
        return false;
    }

    auto tx = static_cast<gpio_num_t>(pin.huawei_tx);
    auto rx = static_cast<gpio_num_t>(pin.huawei_rx);
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(tx, rx, TWAI_MODE_NORMAL);
    g_config.rx_queue_len = 16;

    // interrupts at level 1 are in high demand, at least on ESP32-S3 boards,
    // but only a limited amount can be allocated. failing to allocate an
    // interrupt in the TWAI driver will cause a bootloop. we therefore
    // register the TWAI driver's interrupt at level 2. level 2 interrupts
    // should be available -- we don't really know. we would love to have the
    // esp_intr_dump() function, but that's not available yet in our version
    // of the underlying esp-idf.
    g_config.intr_flags = ESP_INTR_FLAG_LEVEL2;

    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        MessageOutput.print("[Huawei::TWAI] Failed to install driver\r\n");
        return false;
    }

    if (twai_start() != ESP_OK) {
        MessageOutput.print("[Huawei::TWAI] Failed to start driver\r\n");
        return false;
    }

    MessageOutput.print("[Huawei::TWAI] driver ready\r\n");

    return startLoop();
}

bool TWAI::getMessage(HardwareInterface::can_message_t& msg)
{
    twai_status_info_t status;

    while (true) {
        if (twai_get_status_info(&status) != ESP_OK) {
            MessageOutput.print("[Huawei::TWAI] Failed to get status info\r\n");
            return false;
        }

        if (status.msgs_to_rx == 0) { return false; }

        twai_message_t rxMessage;

        // wait for message to be received, function is blocking (for 100ms)
        if (twai_receive(&rxMessage, pdMS_TO_TICKS(100)) != ESP_OK) {
            MessageOutput.print("[Huawei::TWAI] Failed to receive message\r\n");
            return false;
        }

        if (rxMessage.extd != 1) { continue; } // we only process extended format messages

        if (rxMessage.data_length_code != 8) { continue; }

        msg.canId = rxMessage.identifier;
        msg.valueId = rxMessage.data[0] << 24 | rxMessage.data[1] << 16 | rxMessage.data[2] << 8 | rxMessage.data[3];
        msg.value = rxMessage.data[4] << 24 | rxMessage.data[5] << 16 | rxMessage.data[6] << 8 | rxMessage.data[7];

        return true;
    }

    return false;
}

bool TWAI::sendMessage(uint32_t canId, std::array<uint8_t, 8> const& data)
{
    twai_message_t txMsg;
    memset(&txMsg, 0, sizeof(txMsg));
    memcpy(txMsg.data, data.data(), data.size());
    txMsg.extd = 1;
    txMsg.data_length_code = data.size();
    txMsg.identifier = canId;

    return twai_transmit(&txMsg, pdMS_TO_TICKS(1000)) == ESP_OK;
}

} // namespace GridCharger::Huawei
