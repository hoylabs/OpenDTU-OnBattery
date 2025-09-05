// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Malte Schmidt and others
 */
#include <gridcharger/Manager.h>
#include "Configuration.h"
#include <LogHelper.h>

static const char* TAG = "gridChargerManager";

GridChargers::Manager GridChargerManager;

namespace GridChargers {

void Manager::init(Scheduler& scheduler)
{
    DTU_LOGI("Initialize GridCharger Manager...");
    
    // Initialize controllers for maximum number of chargers
    _controllers.reserve(GRIDCHARGER_MAX_COUNT);
    for (uint8_t i = 0; i < GRIDCHARGER_MAX_COUNT; i++) {
        _controllers.emplace_back(std::make_unique<GridChargers::Huawei::Controller>());
    }
    
    updateSettings();
    
    // Initialize only enabled controllers
    for (uint8_t i = 0; i < GRIDCHARGER_MAX_COUNT; i++) {
        auto const& config = Configuration.get();
        if (config.GridCharger[i].Enabled) {
            _controllers[i]->init(scheduler);
        }
    }
}

void Manager::updateSettings()
{
    auto const& config = Configuration.get();
    _enabledCount = 0;
    
    for (uint8_t i = 0; i < GRIDCHARGER_MAX_COUNT; i++) {
        if (config.GridCharger[i].Enabled) {
            _enabledCount++;
            if (_controllers[i]) {
                _controllers[i]->updateSettings();
            }
        }
    }
    
    DTU_LOGI("GridCharger Manager: %d chargers enabled", _enabledCount);
}

GridChargers::Huawei::Controller* Manager::getController(uint8_t id)
{
    if (id >= GRIDCHARGER_MAX_COUNT) {
        return nullptr;
    }
    
    auto const& config = Configuration.get();
    if (!config.GridCharger[id].Enabled) {
        return nullptr;
    }
    
    return _controllers[id].get();
}

uint8_t Manager::getChargerCount() const
{
    return _enabledCount;
}

void Manager::getAllJsonData(JsonVariant& root) const
{
    JsonArray chargers = root["chargers"].to<JsonArray>();
    
    for (uint8_t i = 0; i < GRIDCHARGER_MAX_COUNT; i++) {
        auto const& config = Configuration.get();
        if (config.GridCharger[i].Enabled && _controllers[i]) {
            JsonObject charger = chargers.add<JsonObject>();
            charger["id"] = i;
            _controllers[i]->getJsonData(charger);
        }
    }
    
    root["count"] = _enabledCount;
}

void Manager::getJsonData(uint8_t id, JsonVariant& root) const
{
    auto controller = const_cast<Manager*>(this)->getController(id);
    if (controller) {
        controller->getJsonData(root);
        root["id"] = id;
    } else {
        root["error"] = "Charger not found or disabled";
        root["id"] = id;
    }
}

void Manager::setFan(uint8_t id, bool online, bool fullSpeed)
{
    auto controller = getController(id);
    if (controller) {
        controller->setFan(online, fullSpeed);
    }
}

void Manager::setProduction(uint8_t id, bool enable)
{
    auto controller = getController(id);
    if (controller) {
        controller->setProduction(enable);
    }
}

void Manager::setParameter(uint8_t id, float val, GridChargers::Huawei::HardwareInterface::Setting setting)
{
    auto controller = getController(id);
    if (controller) {
        controller->setParameter(val, setting);
    }
}

void Manager::setMode(uint8_t id, uint8_t mode)
{
    auto controller = getController(id);
    if (controller) {
        controller->setMode(mode);
    }
}

} // namespace GridChargers