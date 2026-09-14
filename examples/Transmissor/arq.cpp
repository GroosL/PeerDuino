#include "arq.hpp"

ARQ *g_arq = nullptr;

#if !defined(ARDUINO) && !defined(__AVR__)
#include <chrono>
uint32_t millis() {
  static auto start = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  return static_cast<uint32_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(now - start)
          .count());
}
#endif

ARQ::ARQ(Stream &port, uint8_t my_id, uint8_t peer_id)
    : port(port), my_id(my_id), peer_id(peer_id), tx_seq(0), rx_seq(0),
      rx_synced(false), timeout_ms(300), max_retries(4) {
  parser.reset();
  g_arq = this;
}

void ARQ::begin() {
  tx_seq = 0;
  rx_seq = 0;
  rx_synced = false;
  parser.reset();
  g_arq = this;
}

void ARQ::sendAck(uint8_t seq) {
  Frame ack{};
  ack.src = my_id;
  ack.dst = peer_id;
  ack.frame_type = FRAME_ACK;
  ack.data_type = TYPE_NONE;
  ack.seq = seq;
  ack.length = 0;

  uint8_t wire[32];
  size_t len = serialize(ack, wire);
  for (size_t i = 0; i < len; ++i) {
    port.write(wire[i]);
  }
}

bool ARQ::sendRaw(uint8_t data_type, const uint8_t *payload, uint8_t length) {
  Frame tx{};
  tx.src = my_id;
  tx.dst = peer_id;
  tx.frame_type = FRAME_DATA;
  tx.data_type = data_type;
  tx.seq = tx_seq;
  tx.length = (length > MAX_PAYLOAD) ? MAX_PAYLOAD : length;
  if (payload && tx.length > 0) {
    memcpy(tx.payload, payload, tx.length);
  }

  uint8_t wire[150];
  size_t wire_len = serialize(tx, wire);
  if (wire_len == 0)
    return false;

  for (uint8_t attempt = 1; attempt <= max_retries; ++attempt) {
    // 1. Envia todos os bytes do quadro (corrompe apenas a 1a tentativa se
    // solicitado)
    for (size_t i = 0; i < wire_len; ++i) {
      uint8_t b = wire[i];
      if (corrupt_next_crc && i == wire_len - 2) {
        b ^= 0xFF;
        corrupt_next_crc = false;
      }
      port.write(b);
    }

    // 2. Aguarda ACK com timeout
    uint32_t start = millis();
    Frame ack{};
    while (millis() - start < timeout_ms) {
      while (port.available() > 0) {
        uint8_t b = static_cast<uint8_t>(port.read());
        if (parser.feed(b, ack)) {
          if (ack.frame_type == FRAME_ACK && ack.src == peer_id &&
              ack.dst == my_id && ack.seq == tx_seq) {
            tx_seq++;
            return true; // Sucesso!
          }
        }
      }
    }
    // Timeout: tenta novamente na próxima iteração
  }

  return false; // Falha após esgotar tentativas
}

bool ARQ::sendByte(uint8_t value) { return sendRaw(TYPE_BYTE, &value, 1); }

bool ARQ::sendWord(uint16_t value) {
  uint8_t buf[2];
  buf[0] = static_cast<uint8_t>(value & 0xFF);
  buf[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
  return sendRaw(TYPE_WORD, buf, 2);
}

bool ARQ::sendFloat(float value) {
  uint8_t buf[4];
  memcpy(buf, &value, sizeof(float));
  return sendRaw(TYPE_FLOAT, buf, 4);
}

bool ARQ::sendData(const uint8_t *data, uint16_t size) {
  if (size == 0) {
    return sendRaw(TYPE_DATA, nullptr, 0);
  }
  if (size <= MAX_PAYLOAD) {
    return sendRaw(TYPE_DATA, data, static_cast<uint8_t>(size));
  }
  // Fragmenta em blocos de no máximo MAX_PAYLOAD bytes
  uint16_t sent = 0;
  while (sent < size) {
    uint8_t chunk = (size - sent > MAX_PAYLOAD)
                        ? MAX_PAYLOAD
                        : static_cast<uint8_t>(size - sent);
    if (!sendRaw(TYPE_DATA, data + sent, chunk)) {
      return false;
    }
    sent += chunk;
  }
  return true;
}

bool ARQ::receive(Frame &out) {
  while (port.available() > 0) {
    uint8_t b = static_cast<uint8_t>(port.read());
    Frame rx{};
    if (parser.feed(b, rx)) {
      if (rx.dst != my_id)
        continue;

      if (rx.frame_type == FRAME_DATA) {
        // Simulação de perda de ACK
        if (drop_next_ack) {
          drop_next_ack = false;
          continue;
        }

        // Responde ACK imediatamente
        sendAck(rx.seq);

        // Sincroniza o primeiro seq recebido
        if (!rx_synced) {
          rx_seq = rx.seq;
          rx_synced = true;
        }

        // Se for o número de sequência esperado, entrega à aplicação
        if (rx.seq == rx_seq) {
          rx_seq++;
          out = rx;
          return true;
        }
        // Se for duplicata, o ACK já foi reenviado; não duplica a entrega
      }
    }
  }
  return false;
}
