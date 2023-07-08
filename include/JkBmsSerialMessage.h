#pragma once

#include <vector>
#include <Arduino.h>

#include "JkBmsDataPoints.h"

namespace JkBms {

class JkBmsSerialMessage {
    public:
        using tData = std::vector<uint8_t>;

        JkBmsSerialMessage() = delete;

        JkBmsSerialMessage(tData const& raw);

        enum class Command : uint8_t {
            Activate = 0x01,
            Write = 0x02,
            Read = 0x03,
            Password = 0x05,
            ReadAll = 0x06
        };

        JkBmsSerialMessage(Command cmd);
        Command getCommand() const { return static_cast<Command>(_raw[8]); }

        enum class Source : uint8_t {
            BMS = 0x00,
            Bluetooth = 0x01,
            GPS = 0x02,
            Host = 0x03
        };
        Source getSource() const { return static_cast<Source>(_raw[9]); }

        enum class Type : uint8_t {
            Command = 0x00,
            Response = 0x01,
            Unsolicited = 0x02
        };
        Type getType() const { return static_cast<Type>(_raw[10]); }

        // this does *not* include the two byte start marker
        uint16_t getFrameLength() const { return get<uint16_t>(_raw.cbegin()+2); }

        uint32_t getTerminalId() const { return get<uint32_t>(_raw.cbegin()+4); }

        // there are 20 bytes of overhead. two of those are the start marker
        // bytes, which are *not* counted by the frame length.
        uint16_t getVariableFieldLength() const { return getFrameLength() - 18; }

        // the upper byte of the 4-byte "record number" is reserved (for encryption)
        uint32_t getSequence() const { return get<uint32_t>(_raw.cend()-9) >> 8; }

        bool isValid() const;

        uint8_t const* data() { return _raw.data(); }
        size_t size() { return _raw.size(); }

    private:
        template<typename T, typename It> T get(It&& pos) const;
        template<typename It> bool getBool(It&& pos) const;
        template<typename It> int16_t getTemperature(It&& pos) const;
        template<typename It> std::string getString(It&& pos, size_t len, bool replaceZeroes = false) const;
        template<typename T> void set(tData::iterator const& pos, T val);
        uint16_t calcChecksum() const;
        void updateChecksum();

        tData _raw;
        JkBms::DataPointContainer _dp;

        static constexpr uint16_t startMarker = 0x4e57;
        static constexpr uint8_t endMarker = 0x68;
};

} /* namespace JkBms */
