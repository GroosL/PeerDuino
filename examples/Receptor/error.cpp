#include "error.hpp"

uint16_t crc16_update(uint16_t crc, uint8_t byte) {
  for (int i = 7; i >= 0; --i) {
    uint8_t bit = static_cast<uint8_t>((byte >> i) & 1);
    uint8_t msb = static_cast<uint8_t>(((crc >> 15) & 1) ^ bit);
    crc = static_cast<uint16_t>(crc << 1);
    if (msb) {
      crc ^= 0x1021;
    }
  }
  return crc;
}

uint16_t crc16(const uint8_t *data, size_t length) {
  uint16_t crc = 0;
  for (size_t i = 0; i < length; ++i) {
    crc = crc16_update(crc, data[i]);
  }
  return crc;
}

bool verify_crc(const uint8_t *data, size_t length, uint16_t received_crc) {
  return crc16(data, length) == received_crc;
}
