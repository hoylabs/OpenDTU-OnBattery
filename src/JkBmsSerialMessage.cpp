#include <numeric>

#include "JkBmsSerialMessage.h"
#include "MessageOutput.h"

JkBmsSerialMessage::JkBmsSerialMessage(Command cmd)
    : _raw(20, 0x00)
{
    set(_raw.begin(), startMarker);
    set(_raw.begin() + 2, static_cast<uint16_t>(_raw.size() - 2)); // frame length
    set(_raw.begin() + 8, static_cast<uint8_t>(cmd));
    set(_raw.begin() + 9, static_cast<uint8_t>(Source::Host));
    set(_raw.begin() + 10, static_cast<uint8_t>(Type::Command));
    set(_raw.end() - 5, endMarker);
    updateChecksum();
}

template<typename T>
T JkBmsSerialMessage::get(tData::const_iterator const& pos) const
{
    // avoid out-of-bound read
    if (std::distance(pos, _raw.end()) < sizeof(T)) { return 0; }

    T res = 0;
    for (unsigned i = 0; i < sizeof(T); ++i) {
        res |= static_cast<T>(*(pos+i)) << (sizeof(T)-1-i)*8;
    }
    return res;
}

template<typename T>
void JkBmsSerialMessage::set(tData::iterator const& pos, T val)
{
    // avoid out-of-bound write
    if (std::distance(pos, _raw.end()) < sizeof(T)) { return; }

    for (unsigned i = 0; i < sizeof(T); ++i) {
        *(pos+i) = static_cast<uint8_t>(val >> (sizeof(T)-1-i)*8);
    }
}

uint16_t JkBmsSerialMessage::calcChecksum() const
{
    return std::accumulate(_raw.begin(), _raw.end()-4, 0);
}

void JkBmsSerialMessage::updateChecksum()
{
    set(_raw.end()-2, calcChecksum());
}

bool JkBmsSerialMessage::isValid() const {
    uint16_t const actualStartMarker = get<uint16_t>(_raw.begin());
    if (actualStartMarker != startMarker) {
        MessageOutput.printf("JkBmsSerialMessage: invalid start marker %04x, expected 0x%04x\r\n",
            actualStartMarker, startMarker);
        return false;
    }

    uint8_t const actualEndMarker = *(_raw.end()-5);
    if (actualEndMarker != endMarker) {
        MessageOutput.printf("JkBmsSerialMessage: invalid end marker %02x, expected 0x%02x\r\n",
            actualEndMarker, endMarker);
        return false;
    }

    uint16_t const actualChecksum = get<uint16_t>(_raw.end()-2);
    uint16_t const expectedChecksum = calcChecksum();
    if (actualChecksum != expectedChecksum) {
        MessageOutput.printf("JkBmsSerialMessage: invalid checksum 0x%04x, expected 0x%04x\r\n",
            actualChecksum, expectedChecksum);
        return false;
    }

    return true;
}
