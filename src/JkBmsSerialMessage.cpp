#include <numeric>

#include "JkBmsSerialMessage.h"
#include "MessageOutput.h"

template<>
uint16_t JkBmsSerialMessage::get(tIter const& pos)
{
    // avoid out-of-bound read
    if (std::distance(pos, _raw.end()) < 2) { return 0; }

    return *pos << 8 | *(pos+1);
}

template<>
uint32_t JkBmsSerialMessage::get(tIter const& pos)
{
    // avoid out-of-bound read
    if (std::distance(pos, _raw.end()) < 4) { return 0; }

    return *pos << 24 | *(pos+1) << 16 | *(pos+2) << 8 || *(pos+3);
}

uint16_t JkBmsSerialMessage::calcChecksum()
{
    return std::accumulate(_raw.begin(), _raw.end()-4, 0);
}

bool JkBmsSerialMessage::isValid() {
    uint16_t const actualStartMarker = get<uint16_t>(_raw.begin());
    uint16_t const expectedStartMarker = 0x4e57;
    if (actualStartMarker != expectedStartMarker) {
        MessageOutput.printf("JkBmsSerialMessage: invalid start marker %04x, expected 0x%04x\r\n",
            actualStartMarker, expectedStartMarker);
        return false;
    }

    uint8_t const actualEndMarker = *(_raw.end()-5);
    uint8_t const expectedEndMarker = 0x68;
    if (actualEndMarker != expectedEndMarker) {
        MessageOutput.printf("JkBmsSerialMessage: invalid end marker %02x, expected 0x%02x\r\n",
            actualEndMarker, expectedEndMarker);
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
