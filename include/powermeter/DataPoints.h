#pragma once

#include <cstdint>
#include <DataPoints.h>

namespace PowerMeters {

enum class DataPointLabel {
    PowerTotal,
    PowerL1,
    PowerL2,
    PowerL3,
    VoltageL1,
    VoltageL2,
    VoltageL3,
    CurrentL1,
    CurrentL2,
    CurrentL3,
    Import,
    Export,
};

template<DataPointLabel> struct DataPointLabelTraits;

#define LABEL_TRAIT(n, u) template<> struct DataPointLabelTraits<DataPointLabel::n> { \
    using type = float; \
    static constexpr char const name[] = #n; \
    static constexpr char const unit[] = u; \
};

LABEL_TRAIT(PowerTotal,      "W");
LABEL_TRAIT(PowerL1,         "W");
LABEL_TRAIT(PowerL2,         "W");
LABEL_TRAIT(PowerL3,         "W");
LABEL_TRAIT(VoltageL1,       "V");
LABEL_TRAIT(VoltageL2,       "V");
LABEL_TRAIT(VoltageL3,       "V");
LABEL_TRAIT(CurrentL1,       "A");
LABEL_TRAIT(CurrentL2,       "A");
LABEL_TRAIT(CurrentL3,       "A");
LABEL_TRAIT(Import,          "kWh");
LABEL_TRAIT(Export,          "kWh");
#undef LABEL_TRAIT

} // namespace PowerMeters

template class DataPointContainer<DataPoint<float>,
                                  PowerMeters::DataPointLabel,
                                  PowerMeters::DataPointLabelTraits>;

namespace PowerMeters {
    using DataPointContainer = DataPointContainer<DataPoint<float>, DataPointLabel, DataPointLabelTraits>;
} // namespace PowerMeters
