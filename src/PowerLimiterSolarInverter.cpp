#include "MessageOutput.h"
#include "PowerLimiterSolarInverter.h"

PowerLimiterSolarInverter::PowerLimiterSolarInverter(bool verboseLogging, PowerLimiterInverterConfig const& config)
    : PowerLimiterOverscalingInverter(verboseLogging, config) { }

uint16_t PowerLimiterSolarInverter::getMaxReductionWatts(bool) const
{
    if (!isEligible()) { return 0; }

    if (!isProducing()) { return 0; }

    auto low = std::min(getCurrentLimitWatts(), getCurrentOutputAcWatts());
    if (low <= _config.LowerPowerLimit) { return 0; }

    return getCurrentOutputAcWatts() - _config.LowerPowerLimit;
}

uint16_t PowerLimiterSolarInverter::getMaxIncreaseWatts() const
{
    if (!isEligible()) { return 0; }

    if (!isProducing()) {
        // the inverter is not producing, we don't know how much we can increase
        // the power, so we return the maximum possible increase
        return getConfiguredMaxPowerWatts();
    }

    // The inverter produces the configured max power or more.
    if (getCurrentOutputAcWatts() >= getConfiguredMaxPowerWatts()) { return 0; }

    // The limit is already at the max or higher.
    if (getCurrentLimitWatts() >= getInverterMaxPowerWatts()) { return 0; }

    float requiredOutputThreshold = calculateRequiredOutputThreshold(getCurrentLimitWatts());

    // when overscaling is NOT in use
    if (!overscalingEnabled()) {
        // when the limit is already at the max power or higher,
        // we can't increase the power.
        if (getCurrentLimitWatts() >= getConfiguredMaxPowerWatts()) { return 0; }

        // if the inverter's output is within the limit, we can increase the power
        if (getCurrentOutputAcWatts() >= getCurrentLimitWatts() * requiredOutputThreshold) {
            return getConfiguredMaxPowerWatts() - getCurrentLimitWatts();
        }

        return 0;
    }

    std::vector<MpptNum_t> dcMppts = _spInverter->getMppts();
    size_t dcTotalMppts = dcMppts.size();

    float expectedAcPowerPerMppt = (getCurrentLimitWatts() / dcTotalMppts) * requiredOutputThreshold;

    // we use the inverter's max power, because each MPPT can deliver its max power individually
    uint16_t inverterMaxPower = getInverterMaxPowerWatts();
    uint16_t maxPowerPerMppt = inverterMaxPower / dcTotalMppts;

    size_t dcNonShadedMppts = 0;
    uint16_t nonShadedMaxIncrease = 0;

    for (auto& m : dcMppts) {
        float mpptPowerAC = calculateMpptPowerAC(m);

        if (mpptPowerAC >= expectedAcPowerPerMppt) {
            if (maxPowerPerMppt > mpptPowerAC) {
                nonShadedMaxIncrease += maxPowerPerMppt - mpptPowerAC;
            }
            dcNonShadedMppts++;
        }
    }

    uint16_t maxOutputIncrease = getConfiguredMaxPowerWatts() - getCurrentOutputAcWatts();
    uint16_t maxLimitIncrease = getInverterMaxPowerWatts() - getCurrentLimitWatts();

    // find the max total increase
    uint16_t maxTotalIncrease = std::min(maxOutputIncrease, maxLimitIncrease);

    // calculated increase should not exceed the max total increase
    return std::min(maxTotalIncrease, nonShadedMaxIncrease);
}

uint16_t PowerLimiterSolarInverter::applyReduction(uint16_t reduction, bool)
{
    if (!isEligible()) { return 0; }

    if (reduction == 0) { return 0; }

    uint16_t baseline = getCurrentLimitWatts();

    // when overscaling is in use we must not use the current limit
    // because it might be scaled.
    if (overscalingEnabled()) {
        baseline = getCurrentOutputAcWatts();
    }

    if ((baseline - _config.LowerPowerLimit) >= reduction) {
        setAcOutput(baseline - reduction);
        return reduction;
    }

    setAcOutput(_config.LowerPowerLimit);
    return getCurrentOutputAcWatts() - _config.LowerPowerLimit;
}

uint16_t PowerLimiterSolarInverter::standby()
{
    // solar-powered inverters are never actually put into standby (by the
    // DPL), but only set to the configured lower power limit instead.
    setAcOutput(_config.LowerPowerLimit);
    return getCurrentOutputAcWatts() - _config.LowerPowerLimit;
}
