#include "MessageOutput.h"
#include "PowerLimiterSolarInverter.h"

PowerLimiterSolarInverter::PowerLimiterSolarInverter(bool verboseLogging, PowerLimiterInverterConfig const& config)
    : PowerLimiterInverter(verboseLogging, config) { }

uint16_t PowerLimiterSolarInverter::getMaxReductionWatts(bool) const
{
    if (isEligible() != Eligibility::Eligible) { return 0; }

    auto low = std::min(getCurrentLimitWatts(), getCurrentOutputAcWatts());
    if (low <= _config.LowerPowerLimit) { return 0; }

    return getCurrentOutputAcWatts() - _config.LowerPowerLimit;
}

uint16_t PowerLimiterSolarInverter::getMaxIncreaseWatts() const
{
    if (isEligible() != Eligibility::Eligible) { return 0; }

    // the maximum increase possible for this inverter
    int16_t maxTotalIncrease = getConfiguredMaxPowerWatts() - getCurrentOutputAcWatts();

    if (!isProducing()) {
        // the inverter is not producing, we don't know how much we can increase
        // the power, so we return the maximum possible increase
        return maxTotalIncrease;
    }

    // when the current limit is less than 15% of the max power of the inverter
    // the output will not match the limit as the inverters are not able to work
    // with those low limits. In this case we assume that the inverter is able to
    // provide more power and we return the maximum possible increase.
    // thanks spcqike for creating a table that can be found here:
    // https://github.com/hoylabs/OpenDTU-OnBattery/issues/1087#issuecomment-2216787552
    if (getCurrentLimitWatts() < getInverterMaxPowerWatts() * 0.15) { return maxTotalIncrease; }

    auto pStats = _spInverter->Statistics();
    std::vector<MpptNum_t> dcMppts = _spInverter->getMppts();
    size_t dcTotalMppts = dcMppts.size();

    float inverterEfficiencyFactor = pStats->getChannelFieldValue(TYPE_INV, CH0, FLD_EFF) / 100;

    // with 97% we are a bit less strict than when we scale the limit
    auto expectedPowerPercentage = 0.97;

    // use the scaling threshold as the expected power percentage if lower,
    // but only when overscaling is enabled and the inverter does not support PDL
    if (_config.UseOverscaling && !_spInverter->supportsPowerDistributionLogic()) {
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

    if (dcNonShadedMppts == 0) {
        // all mppts are shaded, we can't increase the power
        return 0;
    }

    if (dcNonShadedMppts == dcTotalMppts) {
        // no MPPT is shaded, we assume that we can increase the power by the maximum
        return maxTotalIncrease;
    }

    int16_t maxPowerPerMppt = getConfiguredMaxPowerWatts() / dcTotalMppts;

    int16_t currentPowerPerNonShadedMppt = nonShadedMpptACPowerSum / dcNonShadedMppts;

    int16_t maxIncreasePerNonShadedMppt = maxPowerPerMppt - currentPowerPerNonShadedMppt;

    // maximum increase based on the non-shaded mppts
    return maxIncreasePerNonShadedMppt * dcNonShadedMppts;
}

uint16_t PowerLimiterSolarInverter::applyReduction(uint16_t reduction, bool)
{
    if (isEligible() != Eligibility::Eligible) { return 0; }

    if (reduction == 0) { return 0; }

    if ((getCurrentOutputAcWatts() - _config.LowerPowerLimit) >= reduction) {
        setAcOutput(getCurrentOutputAcWatts() - reduction);
        return reduction;
    }

    setAcOutput(_config.LowerPowerLimit);
    return getCurrentOutputAcWatts() - _config.LowerPowerLimit;
}

uint16_t PowerLimiterSolarInverter::applyIncrease(uint16_t increase)
{
    if (isEligible() != Eligibility::Eligible) { return 0; }

    if (increase == 0) { return 0; }

    // do not wake inverter up if it would produce too much power
    if (!isProducing() && _config.LowerPowerLimit > increase) { return 0; }

    // the limit for solar-powered inverters might be scaled, so we use the
    // current output as the baseline. solar-powered inverters in standby have
    // no output (baseline is zero).
    auto baseline = getCurrentOutputAcWatts();

    auto actualIncrease = std::min(increase, getMaxIncreaseWatts());
    setAcOutput(baseline + actualIncrease);
    return actualIncrease;
}

uint16_t PowerLimiterSolarInverter::standby()
{
    // solar-powered inverters are never actually put into standby (by the
    // DPL), but only set to the configured lower power limit instead.
    setAcOutput(_config.LowerPowerLimit);
    return getCurrentOutputAcWatts() - _config.LowerPowerLimit;
}

uint16_t PowerLimiterSolarInverter::scaleLimit(uint16_t expectedOutputWatts)
{
    // overscalling allows us to compensate for shaded panels by increasing the
    // total power limit, if the inverter is solar powered.
    // this feature should not be used when homyiles 'Power Distribution Logic' is available
    // as the inverter will take care of the power distribution across the MPPTs itself.
    // (added in inverter firmware 01.01.12 on supported models (HMS-1600/1800/2000))
    // When disabled we return the expected output.
    if (!_config.UseOverscaling || _spInverter->supportsPowerDistributionLogic()) { return expectedOutputWatts; }

    // prevent scaling if inverter is not producing, as input channels are not
    // producing energy and hence are detected as not-producing, causing
    // unreasonable scaling.
    if (!isProducing()) { return expectedOutputWatts; }

    auto pStats = _spInverter->Statistics();
    std::vector<ChannelNum_t> dcChnls = _spInverter->getChannelsDC();
    std::vector<MpptNum_t> dcMppts = _spInverter->getMppts();
    size_t dcTotalChnls = dcChnls.size();
    size_t dcTotalMppts = dcMppts.size();

    // if there is only one MPPT available, there is nothing we can do
    if (dcTotalMppts <= 1) { return expectedOutputWatts; }

    // test for a reasonable power limit that allows us to assume that an input
    // channel with little energy is actually not producing, rather than
    // producing very little due to the very low limit.
    if (getCurrentLimitWatts() < dcTotalChnls * 10) { return expectedOutputWatts; }

    auto inverterOutputAC = pStats->getChannelFieldValue(TYPE_AC, CH0, FLD_PAC);

    float inverterEfficiencyFactor = pStats->getChannelFieldValue(TYPE_INV, CH0, FLD_EFF);

    // fall back to hoymiles peak efficiency as per datasheet if inverter
    // is currently not producing (efficiency is zero in that case)
    inverterEfficiencyFactor = (inverterEfficiencyFactor > 0) ? inverterEfficiencyFactor/100 : 0.967;

    auto scalingThreshold = static_cast<float>(_config.ScalingThreshold) / 100.0;
    auto expectedAcPowerPerMppt = (getCurrentLimitWatts() / dcTotalMppts) * scalingThreshold;

    if (_verboseLogging) {
        MessageOutput.printf("%s expected AC power per MPPT %.0f W\r\n",
                _logPrefix, expectedAcPowerPerMppt);
    }

    size_t dcShadedMppts = 0;
    auto shadedChannelACPowerSum = 0.0;

    for (auto& m : dcMppts) {
        float mpptPowerAC = 0.0;
        std::vector<ChannelNum_t> mpptChnls = _spInverter->getChannelsDCByMppt(m);

        for (auto& c : mpptChnls) {
            mpptPowerAC += pStats->getChannelFieldValue(TYPE_DC, c, FLD_PDC) * inverterEfficiencyFactor;
        }

        if (mpptPowerAC < expectedAcPowerPerMppt) {
            dcShadedMppts++;
            shadedChannelACPowerSum += mpptPowerAC;
        }

        if (_verboseLogging) {
            MessageOutput.printf("%s MPPT-%c AC power %.0f W\r\n",
                    _logPrefix, mpptName(m), mpptPowerAC);
        }
    }

    // no shading or the shaded channels provide more power than what
    // we currently need.
    if (dcShadedMppts == 0 || shadedChannelACPowerSum >= expectedOutputWatts) {
        return expectedOutputWatts;
    }

    if (dcShadedMppts == dcTotalMppts) {
        // keep the currentLimit when:
        // - all channels are shaded
        // - currentLimit >= expectedOutputWatts
        // - we get the expected AC power or less and
        if (getCurrentLimitWatts() >= expectedOutputWatts &&
                inverterOutputAC <= expectedOutputWatts) {
            if (_verboseLogging) {
                MessageOutput.printf("%s all mppts are shaded, "
                        "keeping the current limit of %d W\r\n",
                        _logPrefix, getCurrentLimitWatts());
            }

            return getCurrentLimitWatts();

        } else {
            return expectedOutputWatts;
        }
    }

    size_t dcNonShadedMppts = dcTotalMppts - dcShadedMppts;
    uint16_t overScaledLimit = (expectedOutputWatts - shadedChannelACPowerSum) / dcNonShadedMppts * dcTotalMppts;

    if (overScaledLimit <= expectedOutputWatts) { return expectedOutputWatts; }

    if (_verboseLogging) {
        MessageOutput.printf("%s %d/%d mppts are not-producing/shaded, scaling %d W\r\n",
                _logPrefix, dcShadedMppts, dcTotalMppts, overScaledLimit);
    }

    return overScaledLimit;
}

void PowerLimiterSolarInverter::setAcOutput(uint16_t expectedOutputWatts)
{
    setExpectedOutputAcWatts(expectedOutputWatts);
    setTargetPowerLimitWatts(scaleLimit(expectedOutputWatts));
    setTargetPowerState(true);
}
