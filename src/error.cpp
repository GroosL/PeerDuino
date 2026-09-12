#include "error.hpp"

uint16_t crc16(const uint8_t* data, size_t length) {
    uint16_t reg{0}, generator{0x1021};
    for (size_t j = 0; j < length; j++) {
        uint8_t byte = data[j];
        for (int i = 7; i >= 0; i--) {
            uint8_t msb {static_cast<uint8_t>(reg >> 15)};
            uint8_t bit {static_cast<uint8_t>((byte >> i) & 1)};
            reg <<= 1;
            reg |= bit;
            if (msb) reg ^= generator;
        }
    }

    for (int i = 0; i < 16; i++) {
        uint8_t topo {static_cast<uint8_t>(reg >> 15)};
        reg <<= 1;
        if (topo) reg ^= generator;
    }

    return reg;
}

bool verify_crc(const uint8_t* data, size_t length, uint16_t received_crc) {
    return crc16(data, length) == received_crc;
}
