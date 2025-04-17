// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <gridcharger/Stats.h>
#include <gridcharger/huawei/DataPoints.h>

namespace GridChargers::Huawei {

class Stats : public ::GridChargers::Stats {
friend class Provider;

public:
    uint32_t getLastUpdate() const;

    std::optional<float> getInputPower() const;

    void getLiveViewData(JsonVariant& root) const;

protected:
    void mqttPublish() const;

    void updateFrom(DataPointContainer const& dataPoints);

private:
    template<DataPointLabel L>
    void addValueInSection(JsonVariant& root,
        std::string const& section, std::string const& name) const
    {
        auto oVal = _dataPoints.get<L>();
        if (!oVal) { return; }

        ::GridChargers::Stats::addValueInSection(root, section, name, *oVal, DataPointLabelTraits<L>::unit);
    }

    template<DataPointLabel L>
    void addStringInSection(JsonVariant& root,
        std::string const& section, std::string const& name,
        bool translate = true) const
    {
        auto oVal = _dataPoints.get<L>();
        if (!oVal) { return; }

        ::GridChargers::Stats::addStringInSection(root, section, name, *oVal, translate);
    }

    mutable DataPointContainer _dataPoints;
};

} // namespace GridChargers::Huawei
