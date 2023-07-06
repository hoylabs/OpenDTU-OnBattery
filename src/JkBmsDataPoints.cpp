#include "JkBmsDataPoints.h"

namespace JkBms {

template<typename T>
std::string dataPointValueToStr(T const& v) {
    std::ostringstream out;
    out << v;
    return out.str();
}

// explicit instanciations for the above unspecialized implementation
template std::string dataPointValueToStr(int16_t const& v);
template std::string dataPointValueToStr(int32_t const& v);
template std::string dataPointValueToStr(uint16_t const& v);
template std::string dataPointValueToStr(uint32_t const& v);

template<>
std::string dataPointValueToStr(std::string const& v) {
    return v;
}

template<>
std::string dataPointValueToStr(bool const& v) {
    return v?"yes":"no";
}

template<>
std::string dataPointValueToStr(uint8_t const& v) {
    std::ostringstream out;
    // this specialization is required as unsigned char is streamed as a
    // character, rather than being interpreted as a decimal number.
    out << static_cast<unsigned>(v);
    return out.str();
}

} /* namespace JkBms */
