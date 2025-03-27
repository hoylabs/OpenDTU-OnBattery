#include "MessageOutput.h"
#include "PowerLimiterSolarInverter.h"

PowerLimiterSolarInverter::PowerLimiterSolarInverter(bool verboseLogging, PowerLimiterInverterConfig const& config)
    : PowerLimiterOverscalingInverter(verboseLogging, config) { }

uint16_t PowerLimiterSolarInverter::getMaxReductionWatts(bool) const
{
    if (isEligible() != Eligibility::Eligible) { return 0; }

    if (!isProducing()) { return 0; }

    auto low = std::min(getCurrentLimitWatts(), getCurrentOutputAcWatts());
    if (low <= _config.LowerPowerLimit) { return 0; }

    return getCurrentOutputAcWatts() - _config.LowerPowerLimit;
}

uint16_t PowerLimiterSolarInverter::getMaxIncreaseWatts() const
{
    if (isEligible() != Eligibility::Eligible) { return 0; }

    if (!isProducing()) {
        // the inverter is not producing, we don't know how much we can increase
        // the power, so we return the maximum possible increase
        return getConfiguredMaxPowerWatts();
    }

    // The inverter produces the max power or more.
    if (getCurrentOutputAcWatts() >= getConfiguredMaxPowerWatts()) { return 0; }

    // when overscaling is NOT in use and the limit is already at the max power
    // we can't increase the power.
    if (!overscalingEnabled() && getCurrentLimitWatts() >= getConfiguredMaxPowerWatts()) { return 0; }

    auto pStats = _spInverter->Statistics();
    std::vector<MpptNum_t> dcMppts = _spInverter->getMppts();
    size_t dcTotalMppts = dcMppts.size();

    float inverterEfficiencyFactor = pStats->getChannelFieldValue(TYPE_INV, CH0, FLD_EFF) / 100;

    // with 97% we are a bit less strict than when we scale the limit
    auto expectedPowerPercentage = 0.97;

    // use the scaling threshold as the expected power percentage if lower,
    // but only when overscaling is enabled and the inverter does not support PDL
    if (overscalingEnabled()) {
        expectedPowerPercentage = std::min(expectedPowerPercentage, static_cast<float>(_config.ScalingThreshold) / 100.0);
    }

    // x% of the expected power is good enough
    auto expectedAcPowerPerMppt = (getCurrentLimitWatts() / dcTotalMppts) * expectedPowerPercentage;

    size_t dcNonShadedMppts = 0;
    auto nonShadedMpptACPowerSum = 0.0;

    for (auto& m : dcMppts) {
        float mpptPowerAC = 0.0;
        std::vector<ChannelNum_t> mpptChnls = _spInverter->getChannelsDCByMppt(m);

        for (auto& c : mpptChnls) {
            mpptPowerAC += pStats->getChannelFieldValue(TYPE_DC, c, FLD_PDC) * inverterEfficiencyFactor;
        }

        if (mpptPowerAC >= expectedAcPowerPerMppt) {
            nonShadedMpptACPowerSum += mpptPowerAC;
            dcNonShadedMppts++;
        }
    }

    // all mppts are shaded, we can't increase the power
    if (dcNonShadedMppts == 0) {
        return 0;
    }

    // no MPPT is shaded, we assume that we can increase the power by the maximum.
    // based on the current limit, because overscaling should not be active when all
    // MPPTs are NOT shaded.
    if (dcNonShadedMppts == dcTotalMppts) {
        // prevent overflow
        if (getCurrentLimitWatts() > getConfiguredMaxPowerWatts()) { return 0; }
        return getConfiguredMaxPowerWatts() - getCurrentLimitWatts();
    }

    // for inverters without PDL we use the configured max power, because the limit will be divided equally across the MPPTs by the inverter.
    uint16_t inverterMaxPower = getConfiguredMaxPowerWatts();

    // for inverter with PDL or when overscaling is enabled we use the max power of the inverter because each MPPT can deliver its max power.
    if (_spInverter->supportsPowerDistributionLogic() || _config.UseOverscaling) {
        inverterMaxPower = getInverterMaxPowerWatts();
    }

    uint16_t maxPowerPerMppt = inverterMaxPower / dcTotalMppts;

    uint16_t currentPowerPerNonShadedMppt = nonShadedMpptACPowerSum / dcNonShadedMppts;

    uint16_t maxIncreasePerNonShadedMppt = 0;

    // unshaded mppts could produce more power than the max power per MPPT.
    if (maxPowerPerMppt > currentPowerPerNonShadedMppt) {
        maxIncreasePerNonShadedMppt = maxPowerPerMppt - currentPowerPerNonShadedMppt;
    }

    // maximum increase based on the non-shaded mppts, can be higher than maxTotalIncrease for inverters
    // with PDL when getConfiguredMaxPowerWatts() is less than getInverterMaxPowerWatts() divided by
    // the number of used/unshaded MPPTs.
    uint16_t maxIncreaseNonShadedMppts = maxIncreasePerNonShadedMppt * dcNonShadedMppts;

    uint16_t maxTotalIncrease = getConfiguredMaxPowerWatts() - getCurrentLimitWatts();

    // prevent overflow
    if (getCurrentLimitWatts() > getConfiguredMaxPowerWatts()) {
        maxTotalIncrease = 0;
    }

    // when overscaling is in use we must not use the current limit
    // because it might be higher than the configured max power.
    if (overscalingEnabled()) {
        uint16_t maxOutputIncrease = getConfiguredMaxPowerWatts() - getCurrentOutputAcWatts();
        uint16_t maxLimitIncrease = getInverterMaxPowerWatts() - getCurrentLimitWatts();

        // constrains the increase to the limit of the inverter.
        maxTotalIncrease = std::min(maxOutputIncrease, maxLimitIncrease);
    }

    // maximum increase should not exceed the max total increase
    return std::min(maxTotalIncrease, maxIncreaseNonShadedMppts);
}

uint16_t PowerLimiterSolarInverter::applyReduction(uint16_t reduction, bool)
{
    if (isEligible() != Eligibility::Eligible) { return 0; }

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
