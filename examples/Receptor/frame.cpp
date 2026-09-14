#include "frame.hpp"
#include "error.hpp"
#include <cstring>

size_t serialize(const Frame &f, uint8_t *out) {
  if (!out || f.length > MAX_PAYLOAD)
    return 0;

  uint8_t raw[6 + MAX_PAYLOAD];
  raw[0] = f.src;
  raw[1] = f.dst;
  raw[2] = f.frame_type;
  raw[3] = f.data_type;
  raw[4] = f.seq;
  raw[5] = f.length;
  if (f.length > 0) {
    memcpy(raw + 6, f.payload, f.length);
  }

  uint16_t crc = crc16(raw, 6 + f.length);

  size_t pos = 0;
  out[pos++] = 0x7E;

  auto put = [&](uint8_t b) {
    if (b == 0x7E || b == 0x7D) {
      out[pos++] = 0x7D;
      out[pos++] = b ^ 0x20;
    } else {
      out[pos++] = b;
    }
  };

  for (size_t i = 0; i < 6 + f.length; i++) {
    put(raw[i]);
  }
  put(static_cast<uint8_t>(crc >> 8));
  put(static_cast<uint8_t>(crc & 0xFF));
  out[pos++] = 0x7E;

  return pos;
}

void FrameParser::reset() {
  idx = 0;
  escaped = false;
}

bool FrameParser::feed(uint8_t b, Frame &out) {
  if (b == 0x7E) {
    bool valid = false;
    // Cabeçalho (6) + payload + CRC (2)
    if (idx >= 8) {
      uint8_t len = buf[5];
      if (len <= MAX_PAYLOAD && idx == 6 + len + 2) {
        uint16_t rx_crc =
            (static_cast<uint16_t>(buf[6 + len]) << 8) | buf[6 + len + 1];
        if (crc16(buf, 6 + len) == rx_crc) {
          out.src = buf[0];
          out.dst = buf[1];
          out.frame_type = buf[2];
          out.data_type = buf[3];
          out.seq = buf[4];
          out.length = len;
          if (len > 0) {
            memcpy(out.payload, buf + 6, len);
          }
          out.crc = rx_crc;
          valid = true;
        }
      }
    }
    idx = 0;
    escaped = false;
    return valid;
  }

  if (escaped) {
    b ^= 0x20;
    escaped = false;
  } else if (b == 0x7D) {
    escaped = true;
    return false;
  }

  if (idx < sizeof(buf)) {
    buf[idx++] = b;
  } else {
    idx = 0; // overflow, descarta o quadro
  }

  return false;
}

bool deserialize(const uint8_t *data, size_t size, Frame &out) {
  if (!data || size < 2)
    return false;
  FrameParser parser;
  for (size_t i = 0; i < size; i++) {
    if (parser.feed(data[i], out))
      return true;
  }
  return false;
}
