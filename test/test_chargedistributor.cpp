// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for ChargeCurrentDistributor.
// Run with: make test_chargedistributor
//
#include <cassert>
#include <cmath>
#include <iostream>

#include "../include/solarcharger/ChargeCurrentDistributor.h"

using CCD = SolarChargers::ChargeCurrentDistributor;
using CD = CCD::ControllerData;

static constexpr float eps = 0.001f;
static bool near(float a, float b)
{
    return std::fabs(a - b) < eps;
}

// Empty controller list must return an empty vector without crashing.
void testEmptyControllerList()
{
  std::cout << "Testing: empty controller list → empty result" << std::endl;

  auto limits = CCD::distribute(10.0f, 0.0f, {});

  assert(limits.empty());

  std::cout << "  PASSED" << std::endl;
}

void testSingleController()
{
    std::cout << "Testing: single controller, no hw max → full limit" << std::endl;

    auto limits = CCD::distribute(10.0f, 5.0f, { { 5.0f, std::nullopt } });

    assert(limits.size() == 1);
    assert(near(limits[0], 10.0f));

    std::cout << "  PASSED" << std::endl;
}

// when the limit exceeds 95 % of a controller's hardware capacity the near-
// capacity shortcut fires and returns maxCurrent so the controller runs at
// its own hardware maximum. chargeLimit=10 >> maxCurrent=3 → shortcut fires.
void testSingleControllerHwMax()
{
    std::cout << "Testing: single controller, limit >> hw max → maxCurrent returned" << std::endl;

    auto limits = CCD::distribute(10.0f, 5.0f, { { 5.0f, std::make_optional(3.0f) } });

    assert(limits.size() == 1);
    assert(near(limits[0], 3.0f));

    std::cout << "  PASSED" << std::endl;
}

// ---------------------------------------------------------------------------
// Inverter-current correction
// ---------------------------------------------------------------------------

// overallChargeCurrent=8, chargeCurrent=5 → inverterCurrent=3 → adjustedLimit=13
void testInverterCurrentCorrection()
{
    std::cout << "Testing: inverter current correction increases effective limit" << std::endl;

    auto limits = CCD::distribute(10.0f, 5.0f, { { 8.0f, std::nullopt } });

    assert(limits.size() == 1);
    assert(near(limits[0], 13.0f));

    std::cout << "  PASSED" << std::endl;
}

void testNoNegativeInverterCorrection()
{
    std::cout << "Testing: negative inverter current is not applied" << std::endl;

    auto limits = CCD::distribute(10.0f, 8.0f, { { 3.0f, std::nullopt } });

    assert(limits.size() == 1);
    assert(near(limits[0], 10.0f));

    std::cout << "  PASSED" << std::endl;
}

// ---------------------------------------------------------------------------
// Near-capacity shortcut: limit >= 95% of total capacity → all at maxCurrent
// ---------------------------------------------------------------------------

// capacity=20A, limit=19A = 95% → shortcut fires → each controller gets its maxCurrent
void testNearCapacityMaximisesAll()
{
    std::cout << "Testing: limit >= 95% of capacity → maxCurrent returned for all controllers" << std::endl;

    std::vector<CD> controllers = {
        { 5.0f, std::make_optional(8.0f) },
        { 7.0f, std::make_optional(12.0f) },
    };
    auto limits = CCD::distribute(19.0f, 12.0f, controllers);

    assert(limits.size() == 2);
    assert(near(limits[0], 8.0f));
    assert(near(limits[1], 12.0f));

    std::cout << "  PASSED" << std::endl;
}

// capacity=20A, limit=18A = 90% → shortcut does not fire, normal distribution
void testBelowNearCapacityThreshold()
{
    std::cout << "Testing: limit < 95% of capacity → normal distribution used" << std::endl;

    std::vector<CD> controllers = {
        { 5.0f, std::make_optional(10.0f) },
        { 5.0f, std::make_optional(10.0f) },
    };
    auto limits = CCD::distribute(18.0f, 10.0f, controllers);

    assert(limits.size() == 2);
    assert(near(limits[0], 9.0f));
    assert(near(limits[1], 9.0f));

    std::cout << "  PASSED" << std::endl;
}

// Shortcut requires ALL controllers to have maxCurrent; missing one → proportional
void testNearCapacityRequiresAllMaxCurrentKnown()
{
    std::cout << "Testing: near-capacity shortcut skipped when any maxCurrent unknown" << std::endl;

    std::vector<CD> controllers = {
        { 5.0f, std::make_optional(10.0f) },
        { 5.0f, std::nullopt },
    };
    auto limits = CCD::distribute(20.0f, 10.0f, controllers);

    // proportional fallback: equal weights → equal limits
    assert(limits.size() == 2);
    assert(near(limits[0], 10.0f));
    assert(near(limits[1], 10.0f));

    std::cout << "  PASSED" << std::endl;
}

// ---------------------------------------------------------------------------
// Proportional distribution
// ---------------------------------------------------------------------------

// All controllers idle: equal weights → uniform distribution
void testAllIdleUniformDistribution()
{
    std::cout << "Testing: all idle → uniform distribution" << std::endl;

    std::vector<CD> controllers = {
        { 0.0f, std::nullopt },
        { 0.0f, std::nullopt },
        { 0.0f, std::nullopt },
    };
    auto limits = CCD::distribute(12.0f, 0.0f, controllers);

    assert(limits.size() == 3);
    assert(near(limits[0], 4.0f));
    assert(near(limits[1], 4.0f));
    assert(near(limits[2], 4.0f));

    std::cout << "  PASSED" << std::endl;
}

// chargeLimit below reserve floor: each controller gets chargeLimit/N
void testBelowReserveFloor()
{
    std::cout << "Testing: chargeLimit below reserve floor" << std::endl;

    std::vector<CD> controllers = {
        { 5.0f, std::nullopt },
        { 5.0f, std::nullopt },
    };
    auto limits = CCD::distribute(0.6f, 10.0f, controllers);

    assert(limits.size() == 2);
    assert(near(limits[0], 0.3f));
    assert(near(limits[1], 0.3f));

    std::cout << "  PASSED" << std::endl;
}

// Equal currents → equal limits, full budget used
void testTwoControllersEqualShare()
{
    std::cout << "Testing: two controllers, equal share" << std::endl;

    std::vector<CD> controllers = {
        { 5.0f, std::nullopt },
        { 5.0f, std::nullopt },
    };
    auto limits = CCD::distribute(20.0f, 10.0f, controllers);

    assert(limits.size() == 2);
    assert(near(limits[0], 10.0f));
    assert(near(limits[1], 10.0f));
    assert(near(limits[0] + limits[1], 20.0f));

    std::cout << "  PASSED" << std::endl;
}

// 1:3 current ratio → proportional limits
// reserve=1, remaining=19: pre-rounding [5.25, 14.75]
// → floor(52.5)/10=5.2 spill=0.05 → floor(148)/10=14.8
void testTwoControllersProportional()
{
    std::cout << "Testing: proportional 1:3 distribution" << std::endl;

    std::vector<CD> controllers = {
        { 1.0f, std::nullopt },
        { 3.0f, std::nullopt },
    };
    auto limits = CCD::distribute(20.0f, 4.0f, controllers);

    assert(limits.size() == 2);
    assert(near(limits[0], 5.2f));
    assert(near(limits[1], 14.8f));
    assert(near(limits[0] + limits[1], 20.0f));
    assert(limits[1] > limits[0]);

    std::cout << "  PASSED" << std::endl;
}

// Idle controller gets 0.5A effective weight so it receives a share and can ramp up.
// weights: 5A and 0.5A, pre-rounding: [8.682, 1.318]
// floor truncation: [8.6, 1.3]  (float32: accumulated spillover falls just below 1.4)
void testIdleControllerGetsProportionalShare()
{
    std::cout << "Testing: idle controller gets proportional share (not stuck)" << std::endl;

    std::vector<CD> controllers = {
        { 5.0f, std::nullopt },
        { 0.0f, std::nullopt },
    };
    auto limits = CCD::distribute(10.0f, 5.0f, controllers);

    assert(limits.size() == 2);
    assert(near(limits[0], 8.6f));
    assert(near(limits[1], 1.3f));
    assert(limits[1] > 0.5f); // can ramp up

    std::cout << "  PASSED" << std::endl;
}

// Hardware max cap: surplus redistributed to the uncapped controller
// iter1: both→10A → [0] capped at 3 (surplus=7) → iter2: [1] gets 17A
void testHardwareMaxCapSurplusRedistributed()
{
    std::cout << "Testing: hw max cap → surplus redistributed" << std::endl;

    std::vector<CD> controllers = {
        { 5.0f, std::make_optional(3.0f) },
        { 5.0f, std::nullopt },
    };
    auto limits = CCD::distribute(20.0f, 10.0f, controllers);

    assert(limits.size() == 2);
    assert(near(limits[0], 3.0f));
    assert(near(limits[1], 17.0f));
    assert(near(limits[0] + limits[1], 20.0f));

    std::cout << "  PASSED" << std::endl;
}

// Multiple caps: surplus flows to the single uncapped controller
void testMultipleCapsRedistribution()
{
    std::cout << "Testing: multiple hw caps → surplus to uncapped" << std::endl;

    std::vector<CD> controllers = {
        { 5.0f, std::make_optional(4.0f) },
        { 5.0f, std::make_optional(4.0f) },
        { 5.0f, std::nullopt },
    };
    auto limits = CCD::distribute(30.0f, 15.0f, controllers);

    assert(limits.size() == 3);
    assert(near(limits[0], 4.0f));
    assert(near(limits[1], 4.0f));
    assert(near(limits[2], 22.0f));
    assert(near(limits[0] + limits[1] + limits[2], 30.0f));

    std::cout << "  PASSED" << std::endl;
}

// Multi-iteration cascade: redistribution itself causes further capping
// maxCurrent=[3A, 8A, none], limit=20A
// iter1: each→6.667 → [0] caps (surplus=3.667)
// iter2: [1]→8.5 caps (surplus=0.5) → iter3: [2]→9.0   total=20 ✓
void testMultiIterationCascade()
{
    std::cout << "Testing: multi-iteration cascade redistribution" << std::endl;

    std::vector<CD> controllers = {
        { 5.0f, std::make_optional(3.0f) },
        { 5.0f, std::make_optional(8.0f) },
        { 5.0f, std::nullopt },
    };
    auto limits = CCD::distribute(20.0f, 15.0f, controllers);

    assert(limits.size() == 3);
    assert(near(limits[0], 3.0f));
    assert(near(limits[1], 8.0f));
    assert(near(limits[2], 9.0f));
    assert(near(limits[0] + limits[1] + limits[2], 20.0f));

    std::cout << "  PASSED" << std::endl;
}

// Rounding spillover: truncation residual carried forward → exact sum
// 3 equal controllers, limit=10: each pre-rounding=3.333...
// [0]→3.3 spill=0.033 → [1]→3.3 spill=0.067 → [2]→3.4   sum=10.0 ✓
void testRoundingSpilloverMakesSumExact()
{
    std::cout << "Testing: rounding spillover → exact sum" << std::endl;

    std::vector<CD> controllers = {
        { 1.0f, std::nullopt },
        { 1.0f, std::nullopt },
        { 1.0f, std::nullopt },
    };
    auto limits = CCD::distribute(10.0f, 3.0f, controllers);

    assert(limits.size() == 3);
    assert(near(limits[0], 3.3f));
    assert(near(limits[1], 3.3f));
    assert(near(limits[2], 3.4f));
    assert(near(limits[0] + limits[1] + limits[2], 10.0f));

    std::cout << "  PASSED" << std::endl;
}

// ---------------------------------------------------------------------------
// previousLimit as effective weight floor
// ---------------------------------------------------------------------------

// A controller reading 0A with a previousLimit set is treated as a transient
// artifact and weighted by its prior limit so it is not starved.
// currents: 8A and 0A, previousLimit[1]=8A → weights equal → equal shares
void testPreviousLimitUsedAsWeightWhenCurrentIsZero()
{
    std::cout << "Testing: previousLimit used as weight when batteryCurrent=0" << std::endl;

    std::vector<CD> controllers = {
        { 8.0f, std::nullopt, std::nullopt },
        { 0.0f, std::nullopt, std::make_optional(8.0f) },
    };
    auto limits = CCD::distribute(16.0f, 8.0f, controllers);

    assert(limits.size() == 2);
    assert(near(limits[0], 8.0f));
    assert(near(limits[1], 8.0f));
    // Without previousLimit, [1] would get only the 0.5A reserve weight → ~0.9A
    assert(limits[1] > 5.0f);

    std::cout << "  PASSED" << std::endl;
}

// A controller with no previousLimit falls back to the 0.5A reserve weight
void testNoPreviousLimitFallsBackToReserve()
{
    std::cout << "Testing: no previousLimit → reserve floor applies" << std::endl;

    std::vector<CD> controllers = {
        { 8.0f, std::nullopt, std::nullopt },
        { 0.0f, std::nullopt, std::nullopt },
    };
    auto limits = CCD::distribute(16.0f, 8.0f, controllers);

    assert(limits.size() == 2);
    assert(limits[0] > limits[1]); // active controller gets more
    assert(limits[1] > 0.5f);      // idle controller can ramp up
    assert(limits[1] < 3.0f);      // but significantly less than active

    std::cout << "  PASSED" << std::endl;
}

// ---------------------------------------------------------------------------
// Spillover cap-residual forwarding
// ---------------------------------------------------------------------------

// When truncation spillover arrives at a capped controller, any residual from
// the hardware-max cap must be forwarded to the next controller, not lost.
// spillover = l - limits[i]  (using final assigned value, not truncated r)
//
// Setup: [0] uncapped (generates spillover), [1] capped at 3A, [2] uncapped.
// limit=15, currents 9/3/3, no inv. correction.
// Distribution: [0]=8.75, [1]=3.0 (capped), [2]=3.25.  Sum=15.
// Truncation:
//   [0]: l=8.75 → r=8.7, spill=0.05
//   [1]: l=3.05 → r=3.0, limits[1]=min(3.0,3.0)=3.0, spill=0.05  (fix: l-limits[i])
//   [2]: l=3.30 → r=3.3, limits[2]=3.3
//   sum=8.7+3.0+3.3=15.0 ✓
void testSpilloverForwardedPastCappedController()
{
    std::cout << "Testing: spillover forwarded past capped controller → exact sum" << std::endl;

    std::vector<CD> controllers = {
        { 9.0f, std::nullopt },
        { 3.0f, std::make_optional(3.0f) },
        { 3.0f, std::nullopt },
    };
    auto limits = CCD::distribute(15.0f, 15.0f, controllers);

    assert(limits.size() == 3);
    assert(near(limits[1], 3.0f));
    assert(near(limits[0] + limits[1] + limits[2], 15.0f));

    std::cout << "  PASSED" << std::endl;
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

// chargeLimit=0 (battery full/in protection) but controllers are feeding
// inverters with high load. adjustedLimit = 0 + 19 = 19A, which is 95% of
// totalCapacity=20A. the shortcut must NOT fire because chargeLimit=0 — using
// adjustedLimit for the threshold would set controllers to maxCurrent and flood
// the battery with (90 - 19) = 4A despite the 0A charge limit.
// expected: proportional path runs, each controller gets 9.5A → battery gets 0A.
void testNearCapacityViaInverterCorrectionDoesNotFireShortcut()
{
    std::cout << "Testing: chargeLimit=0 + high inverter load does not trigger near-capacity shortcut" << std::endl;

    std::vector<CD> controllers = {
        { 9.5f, std::make_optional(10.0f) },
        { 9.5f, std::make_optional(10.0f) },
    };
    auto limits = CCD::distribute(0.0f, 0.0f, controllers);

    // shortcut must not have fired: limits must not be maxCurrent (10A)
    assert(limits.size() == 2);
    assert(near(limits[0], 9.5f));
    assert(near(limits[1], 9.5f));
    // total equals adjustedLimit (19A), so battery receives 0A
    assert(near(limits[0] + limits[1], 19.0f));

    std::cout << "  PASSED" << std::endl;
}

// ---------------------------------------------------------------------------

int main()
{
    std::cout << "=== ChargeCurrentDistributor tests ===" << std::endl;

    testEmptyControllerList();
    testSingleController();
    testSingleControllerHwMax();
    testInverterCurrentCorrection();
    testNoNegativeInverterCorrection();

    testNearCapacityMaximisesAll();
    testBelowNearCapacityThreshold();
    testNearCapacityRequiresAllMaxCurrentKnown();

    testAllIdleUniformDistribution();
    testBelowReserveFloor();
    testTwoControllersEqualShare();
    testTwoControllersProportional();
    testIdleControllerGetsProportionalShare();
    testHardwareMaxCapSurplusRedistributed();
    testMultipleCapsRedistribution();
    testMultiIterationCascade();
    testRoundingSpilloverMakesSumExact();

    testPreviousLimitUsedAsWeightWhenCurrentIsZero();
    testNoPreviousLimitFallsBackToReserve();

    testSpilloverForwardedPastCappedController();

    testNearCapacityViaInverterCorrectionDoesNotFireShortcut();

    std::cout << "All tests passed." << std::endl;
    return 0;
}
