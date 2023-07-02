#pragma once

#include <vector>
#include <Arduino.h>

class JkBmsSerialMessage {
    public:
        using tData = std::vector<uint8_t>;

        JkBmsSerialMessage() = delete;

        JkBmsSerialMessage(tData const& raw)
            : _raw(raw) { }



        bool isValid();

    private:
        using tIter = tData::iterator;

        template<typename T> T get(tIter const& pos);
        uint16_t calcChecksum();

        tData _raw;
};
