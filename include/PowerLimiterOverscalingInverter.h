// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "PowerLimiterInverter.h"

class PowerLimiterOverscalingInverter : public PowerLimiterInverter {
public:
    explicit PowerLimiterOverscalingInverter(PowerLimiterInverterConfig const& config);

    uint16_t applyIncrease(uint16_t increase) final;

protected:
    void setAcOutput(uint16_t expectedOutputWatts) final;
    bool overscalingEnabled() const;
    float calculateRequiredOutputThreshold(uint16_t limitWatts) const;
    float calculateMpptPowerAC(MpptNum_t mppt) const;

private:
    uint16_t scaleLimit(uint16_t newExpectedOutputWatts);
};
