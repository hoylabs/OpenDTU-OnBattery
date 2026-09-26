// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <mutex>
#include <battery/Stats.h>
#include <battery/pytes/rs485/DataPoints.h>
#include <battery/pytes/rs485/Parsers.h>

namespace Batteries::Pytes::Rs485 {

class Stats : public ::Batteries::Stats {
public:
    void getLiveViewData(JsonVariant& root) const final;
    void mqttPublish() const final;
    bool getImmediateChargingRequest() const final;

    void updateBatteryData(DataPointContainer const& dp);
    struct ModuleIdentity {
        String serial;
        String hwVersion;
        String swVersion;
        uint8_t nCells;
    };
    // modules with a usable serial (MQTT topic level / HASS id) and their cell count
    std::vector<ModuleIdentity> getModuleIdentities() const;

    void setModuleBasic(Parsers::ModuleBasicResult const& r);
    void setModuleAnalog(Parsers::ModuleAnalogResult const& r);
    void setModuleChgDsg(Parsers::ModuleChgDsgResult const& r);
    void setModuleCells(uint8_t moduleNo, std::vector<CellData> cells);

private:
    void resizeModules(size_t n);

    // the provider loop writes while the async web server task reads
    // (/api/batterylivedata/status), _modules may reallocate in between
    mutable std::mutex _mutex;

    String moduleName(size_t index) const;
    struct ModuleCounts {
        int blockingCharge = 0;
        int blockingDischarge = 0;
    };
    ModuleCounts getModuleCounts() const;
    static bool isOnline(BatteryModule const& mod);
    static std::optional<bool> isBalancing(BatteryModule const& mod);
    static std::optional<float> averageCellTemperature(BatteryModule const& mod);
    std::optional<bool> isBalancing() const;
    std::optional<std::string> extremeCellName(float BatteryModule::*value, uint8_t BatteryModule::*no, bool max) const;

    DataPointContainer _dataPoints;
    std::vector<BatteryModule> _modules;
};

} // namespace Batteries::Pytes::Rs485
