// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <battery/Stats.h>
#include <battery/pytes/rs485/DataPoints.h>

namespace Batteries::Pytes::Rs485 {

class Stats : public ::Batteries::Stats {
public:
    void getLiveViewData(JsonVariant& root) const final;
    void mqttPublish() const final;
    bool getImmediateChargingRequest() const final;

    void updatePackData(DataPointContainer const& dp);

    void resizeModules(size_t n);
    void setModuleBasic(uint8_t moduleNo, String hwVersion, String swVersion, String serial);
    void setModuleAnalog(uint8_t moduleNo, float voltageV, float currentA, float soc, uint16_t health,
                         uint32_t totalCapacityMah, uint32_t remainCapacityMah,
                         float ambientTemp, int chargeCycles, uint16_t balance,
                         float cellMaxV, uint8_t cellMaxNo, float cellMinV, uint8_t cellMinNo,
                         float tempMaxC, uint8_t tempMaxNo, float tempMinC, uint8_t tempMinNo);
    void setModuleChgDsg(uint8_t moduleNo, float maxChgVoltV, float minDsgVoltV, float maxChgCurrA, float maxDsgCurrA, bool fullChgReq, uint8_t emergFlags);
    void setModuleCells(uint8_t moduleNo, std::vector<CellData> cells);
    void setModuleProtect(uint8_t moduleNo, ProtectParams const& protect);

private:
    DataPointContainer _dataPoints;
    std::vector<BatteryModule> _modules;
    mutable uint32_t _lastMqttPublish = 0;
    mutable uint32_t _lastFullMqttPublish = 0;
};

} // namespace Batteries::Pytes::Rs485
