#pragma once

#include <DataPoints.h>
#include <string>

namespace GridChargers::HTTP {

enum class DataPointLabel : uint8_t {
    AcPowerSetpoint,
    AcPower,
};

template<DataPointLabel> struct DataPointLabelTraits;

#define LABEL_TRAIT(n, t, u) template<> struct DataPointLabelTraits<DataPointLabel::n> { \
    using type = t; \
    static constexpr char const name[] = #n; \
    static constexpr char const unit[] = u; \
};

LABEL_TRAIT(AcPowerSetpoint,        float,       "W");
LABEL_TRAIT(AcPower,                float,       "W");
#undef LABEL_TRAIT

} // namespace GridChargers::HTTP

template class DataPointContainer<DataPoint<float, std::string>,
                                  GridChargers::HTTP::DataPointLabel,
                                  GridChargers::HTTP::DataPointLabelTraits>;

namespace GridChargers::HTTP {
    using DataPointContainer = ::DataPointContainer<::DataPoint<float, std::string>, DataPointLabel, DataPointLabelTraits>;
} // namespace GridChargers::HTTP
