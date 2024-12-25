#pragma once

#include <cstdint>
#include <DataPoints.h>

namespace GridCharger::Huawei {

enum class DataPointLabel : uint8_t {
    InputPower = 0x70,
    InputFrequency = 0x71,
    InputCurrent = 0x72,
    OutputPower = 0x73,
    Efficiency = 0x74,
    OutputVoltage = 0x75,
    OutputCurrentMax = 0x76,
    InputVoltage = 0x78,
    OutputTemperature = 0x7F,
    InputTemperature = 0x80,
    OutputCurrent = 0x81
};

template<DataPointLabel> struct DataPointLabelTraits;

#define LABEL_TRAIT(n, u) template<> struct DataPointLabelTraits<DataPointLabel::n> { \
    using type = float; \
    static constexpr char const name[] = #n; \
    static constexpr char const unit[] = u; \
};

LABEL_TRAIT(InputPower,         "W");
LABEL_TRAIT(InputFrequency,     "Hz");
LABEL_TRAIT(InputCurrent,       "A");
LABEL_TRAIT(OutputPower,        "W");
LABEL_TRAIT(Efficiency,         "%");
LABEL_TRAIT(OutputVoltage,      "V");
LABEL_TRAIT(OutputCurrentMax,   "A");
LABEL_TRAIT(InputVoltage,       "V");
LABEL_TRAIT(OutputTemperature,  "°C");
LABEL_TRAIT(InputTemperature,   "°C");
LABEL_TRAIT(OutputCurrent,      "A");
#undef LABEL_TRAIT

} // namespace GridCharger::Huawei

template class DataPointContainer<DataPoint<float>,
                                  GridCharger::Huawei::DataPointLabel,
                                  GridCharger::Huawei::DataPointLabelTraits>;

namespace GridCharger::Huawei {
    using DataPointContainer = DataPointContainer<DataPoint<float>, DataPointLabel, DataPointLabelTraits>;
} // namespace GridCharger::Huawei
