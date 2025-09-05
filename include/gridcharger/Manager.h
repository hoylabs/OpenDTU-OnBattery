// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <ArduinoJson.h>
#include <TaskSchedulerDeclarations.h>
#include <gridcharger/huawei/Controller.h>
#include "defaults.h"

namespace GridChargers {

class Manager {
public:
    void init(Scheduler& scheduler);
    void updateSettings();
    
    // Get controller by ID (0-based index)
    GridChargers::Huawei::Controller* getController(uint8_t id);
    
    // Get number of enabled chargers
    uint8_t getChargerCount() const;
    
    // Get JSON data for all chargers
    void getAllJsonData(JsonVariant& root) const;
    
    // Get JSON data for specific charger
    void getJsonData(uint8_t id, JsonVariant& root) const;
    
    // Control methods for specific charger
    void setFan(uint8_t id, bool online, bool fullSpeed);
    void setProduction(uint8_t id, bool enable);
    void setParameter(uint8_t id, float val, GridChargers::Huawei::HardwareInterface::Setting setting);
    void setMode(uint8_t id, uint8_t mode);
    
    // Backward compatibility - delegate to first charger
    GridChargers::Huawei::Controller* getFirstController() { return getController(0); }

private:
    std::vector<std::unique_ptr<GridChargers::Huawei::Controller>> _controllers;
    uint8_t _enabledCount = 0;
};

} // namespace GridChargers

extern GridChargers::Manager GridChargerManager;