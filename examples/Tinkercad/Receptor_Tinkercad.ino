#include <SoftwareSerial.h>

// ============================================================================
// CONFIGURAÇÃO DOS PINOS E CONSTANTES
// Pino 10 = RX (conectar ao Pino 11 do Transmissor)
// Pino 11 = TX (conectar ao Pino 10 do Transmissor)
// ============================================================================
SoftwareSerial linkSerial(10, 11);

#define MY_ID        2
#define PEER_ID      1

#define FRAME_DATA   0x01
#define FRAME_ACK    0x02

#define TYPE_BYTE    0x01
#define TYPE_WORD    0x02
#define TYPE_FLOAT   0x03
#define TYPE_DATA    0x04

#define MAX_PAYLOAD  32

// ============================================================================
// ESTADO DO RECEPTOR
// ============================================================================
uint8_t rx_seq = 0;
bool rx_synced = false;
bool drop_next_ack = false;

// Buffer do parser de recepção
uint8_t rx_buf[6 + MAX_PAYLOAD + 2];
uint8_t rx_idx = 0;
bool rx_escaped = false;

// Dados do quadro decodificado
uint8_t recv_src = 0;
uint8_t recv_dst = 0;
uint8_t recv_frame_type = 0;
uint8_t recv_data_type = 0;
uint8_t recv_seq = 0;
uint8_t recv_len = 0;
uint8_t recv_payload[MAX_PAYLOAD];

// ============================================================================
// CRC-16 CCITT (Polinômio 0x1021)
// ============================================================================
uint16_t calcCrc(const uint8_t* data, uint8_t len) {
    uint16_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        for (int bit = 7; bit >= 0; --bit) {
            uint8_t bit_val = (b >> bit) & 1;
            uint8_t msb = ((crc >> 15) & 1) ^ bit_val;
            crc = (uint16_t)(crc << 1);
            if (msb) crc ^= 0x1021;
        }
    }
    return crc;
}

// ============================================================================
// ENVIO DE ACK
// ============================================================================
void sendAck(uint8_t seq) {
    uint8_t raw[6];
    raw[0] = MY_ID;
    raw[1] = PEER_ID;
    raw[2] = FRAME_ACK;
    raw[3] = 0;
    raw[4] = seq;
    raw[5] = 0; // length = 0

    uint16_t crc_val = calcCrc(raw, 6);

    // Delimitador inicial
    linkSerial.write((uint8_t)0x7E);
    delay(2);

    // Cabeçalho
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t b = raw[i];
        if (b == 0x7E || b == 0x7D) {
            linkSerial.write((uint8_t)0x7D);
            delay(2);
            linkSerial.write((uint8_t)(b ^ 0x20));
        } else {
            linkSerial.write(b);
        }
        delay(2);
    }

    // CRC (MSB)
    uint8_t h = (uint8_t)(crc_val >> 8);
    if (h == 0x7E || h == 0x7D) {
        linkSerial.write((uint8_t)0x7D);
        delay(2);
        linkSerial.write((uint8_t)(h ^ 0x20));
    } else {
        linkSerial.write(h);
    }
    delay(2);

    // CRC (LSB)
    uint8_t l = (uint8_t)(crc_val & 0xFF);
    if (l == 0x7E || l == 0x7D) {
        linkSerial.write((uint8_t)0x7D);
        delay(2);
        linkSerial.write((uint8_t)(l ^ 0x20));
    } else {
        linkSerial.write(l);
    }
    delay(2);

    // Delimitador final
    linkSerial.write((uint8_t)0x7E);
    delay(5);
}

// ============================================================================
// PARSER INCREMENTAL DE BYTES RECEBIDOS
// ============================================================================
bool parseRxByte(uint8_t b) {
    if (b == 0x7E) {
        bool valid = false;
        if (rx_idx >= 8) {
            uint8_t len = rx_buf[5];
            if (len <= MAX_PAYLOAD && rx_idx == 6 + len + 2) {
                uint16_t crc_calc = calcCrc(rx_buf, 6 + len);
                uint16_t crc_rx = ((uint16_t)rx_buf[6 + len] << 8) | rx_buf[6 + len + 1];
                if (crc_calc == crc_rx) {
                    recv_src = rx_buf[0];
                    recv_dst = rx_buf[1];
                    recv_frame_type = rx_buf[2];
                    recv_data_type = rx_buf[3];
                    recv_seq = rx_buf[4];
                    recv_len = len;
                    for (uint8_t i = 0; i < len; i++) {
                        recv_payload[i] = rx_buf[6 + i];
                    }
                    valid = true;
                }
            }
        }
        rx_idx = 0;
        rx_escaped = false;
        return valid;
    }

    if (rx_escaped) {
        b ^= 0x20;
        rx_escaped = false;
    } else if (b == 0x7D) {
        rx_escaped = true;
        return false;
    }

    if (rx_idx < sizeof(rx_buf)) {
        rx_buf[rx_idx++] = b;
    } else {
        rx_idx = 0;
    }
    return false;
}

// ============================================================================
// SETUP & LOOP (RECEPTOR)
// ============================================================================
void setup() {
    Serial.begin(9600);
    linkSerial.begin(9600);
    linkSerial.listen();

    pinMode(13, OUTPUT);
    // Pisca 3 vezes para você ver no Tinkercad que o Receptor ligou corretamente!
    for (int i = 0; i < 3; i++) {
        digitalWrite(13, HIGH); delay(100);
        digitalWrite(13, LOW); delay(100);
    }

    Serial.println(F("========================================"));
    Serial.println(F("        PeerDuino - Receptor"));
    Serial.println(F("========================================"));
    Serial.println(F("Aguardando mensagens do Transmissor..."));
    Serial.println(F("Dica: digite 'd' para descartar o proximo ACK"));
    Serial.println(F("========================================\n"));
}

void loop() {
    // Comando interativo para injetar falha de ACK
    if (Serial.available() > 0) {
        char c = Serial.read();
        if (c == 'd' || c == 'D') {
            drop_next_ack = true;
            Serial.println(F("\n[TESTE] O proximo ACK sera DESCARTADO!"));
            Serial.println(F("        Isso demonstrara a retransmissao"));
            Serial.println(F("        e a filtragem de duplicatas.\n"));
        }
    }

    // Leitura contínua da SoftwareSerial
    while (linkSerial.available() > 0) {
        uint8_t b = (uint8_t)linkSerial.read();

        // Acende o LED ao receber sinal
        digitalWrite(13, HIGH);

        if (parseRxByte(b)) {
            // Verifica se é para este nó e se é quadro de DADOS
            if (recv_dst == MY_ID && recv_frame_type == FRAME_DATA) {
                if (drop_next_ack) {
                    drop_next_ack = false;
                    Serial.println(F("[FALHA SIMULADA] ACK descartado propositalmente!"));
                    digitalWrite(13, LOW);
                    continue;
                }

                // Pequeno atraso para dar tempo ao Transmissor de entrar em modo escuta
                delay(20);

                // Envia ACK
                sendAck(recv_seq);
                digitalWrite(13, LOW);

                // Sincroniza sequência no primeiro pacote recebido
                if (!rx_synced) {
                    rx_seq = recv_seq;
                    rx_synced = true;
                }

                // Verifica se é o pacote esperado ou duplicata
                if (recv_seq == rx_seq) {
                    rx_seq++;
                    Serial.print(F("[RX SUCESSO] Seq: "));
                    Serial.print(recv_seq);
                    Serial.print(F(" | Tam: "));
                    Serial.print(recv_len);
                    Serial.print(F(" bytes | "));

                    switch (recv_data_type) {
                        case TYPE_BYTE: {
                            uint8_t val = recv_payload[0];
                            Serial.print(F("Tipo: BYTE | Valor: "));
                            Serial.print(val);
                            Serial.print(F(" (0x"));
                            Serial.print(val, HEX);
                            Serial.println(F(")"));
                            break;
                        }
                        case TYPE_WORD: {
                            uint16_t val = (uint16_t)recv_payload[0] | ((uint16_t)recv_payload[1] << 8);
                            Serial.print(F("Tipo: WORD | Valor: "));
                            Serial.println(val);
                            break;
                        }
                        case TYPE_FLOAT: {
                            float val;
                            uint8_t* p = (uint8_t*)&val;
                            p[0] = recv_payload[0];
                            p[1] = recv_payload[1];
                            p[2] = recv_payload[2];
                            p[3] = recv_payload[3];
                            Serial.print(F("Tipo: FLOAT | Valor: "));
                            Serial.println(val, 5);
                            break;
                        }
                        case TYPE_DATA: {
                            Serial.print(F("Tipo: DADOS | Conteudo: \""));
                            for (uint8_t i = 0; i < recv_len; i++) {
                                Serial.write(recv_payload[i]);
                            }
                            Serial.println(F("\""));
                            break;
                        }
                        default:
                            Serial.println(F("Tipo DESCONHECIDO"));
                            break;
                    }
                } else {
                    Serial.print(F("[DUPLICATA IGNORADA] Seq recebida: "));
                    Serial.print(recv_seq);
                    Serial.print(F(", esperada: "));
                    Serial.print(rx_seq);
                    Serial.println(F(" (ACK reenviado)"));
                }
            }
        }
    }
    delay(10); // Estabilidade do simulador no navegador
}
