// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <optional>
#include <vector>

namespace SolarChargers {

// Distributes a total charge current limit across multiple MPPT controllers.
// No hardware or framework dependencies — testable on the host.
//
// The effective weight for each controller is:
//   max(batteryCurrent, previousLimit, 0.5 A reserve)
//
// The reserve ensures completely idle controllers still get a share and can
// ramp up. previousLimit replaces a transient 0 A reading on a recently-active
// controller so it is not starved by a sampling artifact.
//
// When all hardware maxima are known and the requested limit reaches 95 % of
// total capacity, every controller is set to its maximum immediately to avoid
// register churn and hunting near the top of the operating range.
class ChargeCurrentDistributor {
  public:
    struct ControllerData {
        float batteryCurrent;                  // actual (smoothed) battery charge current in A
        std::optional<float> maxCurrent;       // hardware maximum charge current in A (currentlimit)
        std::optional<float> previousLimit;    // charge current limit set last cycle in A (maxchargecurrent)
                                               // nullopt = no prior limit known
    };

    // Distribute chargeLimit across controllers.
    // chargeLimit   — total charge current limit in A (must not be FLT_MAX)
    // chargeCurrent — actual charge current drawn by the battery (from BMS) in A;
    //                 used to estimate inverter load and adjust the effective limit
    // controllers   — per-controller input data; order matches the returned vector
    // returns         per-controller charge current limits in A, same order as controllers
    static std::vector<float> distribute(float chargeLimit, float chargeCurrent,
                                         std::vector<ControllerData> const& controllers);
};

} // namespace SolarChargers
