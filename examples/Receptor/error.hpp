#pragma once

#include <cstddef>
#include <cstdint>

uint16_t crc16_update(uint16_t crc, uint8_t byte);
uint16_t crc16(const uint8_t *data, size_t length);

bool verify_crc(const uint8_t *data, size_t length, uint16_t received_crc);
