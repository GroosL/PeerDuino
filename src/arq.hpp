#pragma once

#include "frame.hpp"
#include <Arduino.h>

#if defined(ARDUINO) || defined(__AVR__)
#include <Arduino.h>
#include <Stream.h>
#else
// Abstração mínima de Stream para compilação e testes em ambiente desktop
struct Stream {
  virtual int read() = 0;
  virtual int available() = 0;
  virtual size_t write(uint8_t b) = 0;
  virtual ~Stream() = default;
};
uint32_t millis();
#endif

class ARQ {
public:
  ARQ(Stream &port, uint8_t my_id = 1, uint8_t peer_id = 2);

  void begin();

  // Protótipos de envio exigidos pelo professor (Stop-and-Wait síncrono)
  bool sendByte(uint8_t value);
  bool sendWord(uint16_t value);
  bool sendFloat(float value);
  bool sendData(const uint8_t *data, uint16_t size);

  // Recepção não-bloqueante (chamar dentro do loop)
  // Retorna true quando um novo quadro de dados for recebido com sucesso
  bool receive(Frame &out);

  // Envio de confirmação (ACK)
  void sendAck(uint8_t seq);

  // Configurações de temporização e tentativas
  void setTimeout(uint16_t ms) { timeout_ms = ms; }
  void setMaxRetries(uint8_t retries) { max_retries = retries; }

  // Flags para demonstração de falhas ao professor
  bool corrupt_next_crc = false; // corrompe o CRC no envio para simular ruído
  bool drop_next_ack = false;    // receptor não responde ACK para simular perda

  // Utilitários para decodificar tipos recebidos
  static uint8_t getByte(const Frame &f) { return f.payload[0]; }
  static uint16_t getWord(const Frame &f) {
    return static_cast<uint16_t>(f.payload[0]) |
           (static_cast<uint16_t>(f.payload[1]) << 8);
  }
  static float getFloat(const Frame &f) {
    float val;
    memcpy(&val, f.payload, sizeof(float));
    return val;
  }

private:
  bool sendRaw(uint8_t data_type, const uint8_t *payload, uint8_t length);

  Stream &port;
  uint8_t my_id;
  uint8_t peer_id;
  uint8_t tx_seq;
  uint8_t rx_seq;
  bool rx_synced;
  uint16_t timeout_ms;
  uint8_t max_retries;
  FrameParser parser;
};

// Instância global opcional para permitir chamadas diretas como sendByte(...)
extern ARQ *g_arq;

inline bool sendByte(uint8_t value) {
  return g_arq ? g_arq->sendByte(value) : false;
}
inline bool sendWord(uint16_t value) {
  return g_arq ? g_arq->sendWord(value) : false;
}
inline bool sendFloat(float value) {
  return g_arq ? g_arq->sendFloat(value) : false;
}
inline bool sendData(const uint8_t *data, uint16_t size) {
  return g_arq ? g_arq->sendData(data, size) : false;
}
