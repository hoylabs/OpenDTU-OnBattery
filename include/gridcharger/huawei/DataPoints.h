#pragma once

#include <cstdint>
#include <DataPoints.h>

namespace GridCharger::Huawei {

enum class DataPointLabel : uint8_t {
    BoardType,
    Serial,
    Manufactured,
    VendorName,
    ProductName,
    ProductDescription,
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

#define LABEL_TRAIT(n, t, u) template<> struct DataPointLabelTraits<DataPointLabel::n> { \
    using type = t; \
    static constexpr char const name[] = #n; \
    static constexpr char const unit[] = u; \
};

LABEL_TRAIT(BoardType,          std::string, "");
LABEL_TRAIT(Serial,             std::string, "");
LABEL_TRAIT(Manufactured,       std::string, "");
LABEL_TRAIT(VendorName,         std::string, "");
LABEL_TRAIT(ProductName,        std::string, "");
LABEL_TRAIT(ProductDescription, std::string, "");
LABEL_TRAIT(InputPower,         float,       "W");
LABEL_TRAIT(InputFrequency,     float,       "Hz");
LABEL_TRAIT(InputCurrent,       float,       "A");
LABEL_TRAIT(OutputPower,        float,       "W");
LABEL_TRAIT(Efficiency,         float,       "%");
LABEL_TRAIT(OutputVoltage,      float,       "V");
LABEL_TRAIT(OutputCurrentMax,   float,       "A");
LABEL_TRAIT(InputVoltage,       float,       "V");
LABEL_TRAIT(OutputTemperature,  float,       "°C");
LABEL_TRAIT(InputTemperature,   float,       "°C");
LABEL_TRAIT(OutputCurrent,      float,       "A");
#undef LABEL_TRAIT

} // namespace GridCharger::Huawei

template class DataPointContainer<DataPoint<float, std::string>,
                                  GridCharger::Huawei::DataPointLabel,
                                  GridCharger::Huawei::DataPointLabelTraits>;

namespace GridCharger::Huawei {
    using DataPointContainer = DataPointContainer<DataPoint<float, std::string>, DataPointLabel, DataPointLabelTraits>;
} // namespace GridCharger::Huawei
