#pragma once

#include <cstddef>
#include <cstdint>

#define MAX_PAYLOAD 64

// Tipos de quadro
enum FrameType : uint8_t {
  FRAME_DATA = 0x01,
  FRAME_ACK = 0x02,
  FRAME_NAK = 0x03
};

// Tipos de dados (conforme especificação do trabalho)
enum DataType : uint8_t {
  TYPE_NONE = 0x00,
  TYPE_BYTE = 0x01,
  TYPE_WORD = 0x02,
  TYPE_FLOAT = 0x03,
  TYPE_DATA = 0x04
};

struct Frame {
  uint8_t src;
  uint8_t dst;
  uint8_t frame_type;
  uint8_t data_type;
  uint8_t seq;
  uint8_t length;
  uint8_t payload[MAX_PAYLOAD];
  uint16_t crc;
};

// Parser incremental para receber bytes da Serial sem travar a CPU
struct FrameParser {
  uint8_t buf[6 + MAX_PAYLOAD + 2]; // 72 bytes no total
  uint8_t idx = 0;
  bool escaped = false;

  void reset();
  bool feed(uint8_t b, Frame &out);
};

// Serializa o quadro com byte stuffing e delimitadores 0x7E
// Retorna a quantidade de bytes gravados em 'out'
size_t serialize(const Frame &f, uint8_t *out);

// Deserializa um buffer completo
bool deserialize(const uint8_t *data, size_t size, Frame &out);
