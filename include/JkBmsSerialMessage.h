#pragma once

#include <vector>
#include <Arduino.h>

namespace JkBms {

class JkBmsSerialMessage {
    public:
        using tData = std::vector<uint8_t>;

        JkBmsSerialMessage() = delete;

        JkBmsSerialMessage(tData const& raw)
            : _raw(raw) { }

        enum class Command : uint8_t {
            Activate = 0x01,
            Write = 0x02,
            Read = 0x03,
            Password = 0x05,
            ReadAll = 0x06
        };

        JkBmsSerialMessage(Command cmd);

        bool isValid() const;

        enum class Source : uint8_t {
            BMS = 0x00,
            Bluetooth = 0x01,
            GPS = 0x02,
            Host = 0x03
        };

        enum class Type : uint8_t {
            Command = 0x00,
            Response = 0x01,
            Unsolicited = 0x02
        };

        uint8_t const* data() { return _raw.data(); }
        size_t size() { return _raw.size(); }

    private:
        template<typename T> T get(tData::const_iterator const& pos) const;
        template<typename T> void set(tData::iterator const& pos, T val);
        uint16_t calcChecksum() const;
        void updateChecksum();

        tData _raw;

        static constexpr uint16_t startMarker = 0x4e57;
        static constexpr uint8_t endMarker = 0x68;
};

} /* namespace JkBms */
