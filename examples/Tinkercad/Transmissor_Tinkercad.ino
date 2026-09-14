#include <SoftwareSerial.h>

// ============================================================================
// CONFIGURAÇÃO DOS PINOS E CONSTANTES
// Pino 10 = RX (conectar ao Pino 11 do Receptor)
// Pino 11 = TX (conectar ao Pino 10 do Receptor)
// ============================================================================
SoftwareSerial linkSerial(10, 11);

#define MY_ID        1
#define PEER_ID      2

#define FRAME_DATA   0x01
#define FRAME_ACK    0x02

#define TYPE_BYTE    0x01
#define TYPE_WORD    0x02
#define TYPE_FLOAT   0x03
#define TYPE_DATA    0x04

#define MAX_PAYLOAD  32
#define TIMEOUT_MS   1200
#define MAX_RETRIES  4

// ============================================================================
// ESTADO DO TRANSMISSOR
// ============================================================================
uint8_t tx_seq = 0;
bool corrupt_next_crc = false;

// Buffer para decodificar ACK recebido
uint8_t ack_buf[16];
uint8_t ack_idx = 0;
bool ack_escaped = false;

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
// ENVIO COM BYTE STUFFING (0x7E e 0x7D com XOR 0x20)
// ============================================================================
void sendEscaped(uint8_t b) {
    if (b == 0x7E || b == 0x7D) {
        linkSerial.write((uint8_t)0x7D);
        delay(2);
        linkSerial.write((uint8_t)(b ^ 0x20));
    } else {
        linkSerial.write(b);
    }
}

void sendRawFrame(uint8_t src, uint8_t dst, uint8_t f_type, uint8_t d_type, uint8_t seq, const uint8_t* payload, uint8_t len, bool corrupt_crc) {
    uint8_t raw[6 + MAX_PAYLOAD];
    raw[0] = src;
    raw[1] = dst;
    raw[2] = f_type;
    raw[3] = d_type;
    raw[4] = seq;
    raw[5] = len;
    for (uint8_t i = 0; i < len; i++) {
        raw[6 + i] = payload[i];
    }

    uint16_t crc_val = calcCrc(raw, 6 + len);
    if (corrupt_crc) {
        crc_val ^= 0xFFFF; // Inverte CRC para demonstrar falha e retransmissão
    }

    // Delimitador inicial
    linkSerial.write((uint8_t)0x7E);
    delay(2);

    // Cabeçalho e Payload
    for (uint8_t i = 0; i < 6 + len; i++) {
        sendEscaped(raw[i]);
        delay(2);
    }

    // CRC (MSB primeiro, depois LSB)
    sendEscaped((uint8_t)(crc_val >> 8));
    delay(2);
    sendEscaped((uint8_t)(crc_val & 0xFF));
    delay(2);

    // Delimitador final
    linkSerial.write((uint8_t)0x7E);
    delay(5);
}

// ============================================================================
// PROCESSADOR DE ACK (RECEPÇÃO)
// ============================================================================
void resetAckParser() {
    ack_idx = 0;
    ack_escaped = false;
}

bool checkAckByte(uint8_t b, uint8_t expected_seq) {
    if (b == 0x7E) {
        bool valid = false;
        // Quadro ACK válido: 6 bytes cabeçalho (len=0) + 2 bytes CRC = 8 bytes
        if (ack_idx == 8) {
            uint16_t rx_crc = ((uint16_t)ack_buf[6] << 8) | ack_buf[7];
            if (calcCrc(ack_buf, 6) == rx_crc) {
                if (ack_buf[0] == PEER_ID &&
                    ack_buf[1] == MY_ID &&
                    ack_buf[2] == FRAME_ACK &&
                    ack_buf[4] == expected_seq) {
                    valid = true;
                }
            }
        }
        ack_idx = 0;
        ack_escaped = false;
        return valid;
    }

    if (ack_escaped) {
        b ^= 0x20;
        ack_escaped = false;
    } else if (b == 0x7D) {
        ack_escaped = true;
        return false;
    }

    if (ack_idx < sizeof(ack_buf)) {
        ack_buf[ack_idx++] = b;
    } else {
        ack_idx = 0;
    }
    return false;
}

// ============================================================================
// PROTOCOLO ARQ (STOP-AND-WAIT COM TIMEOUT E RETENTATIVAS)
// ============================================================================
bool sendPacket(uint8_t d_type, const uint8_t* payload, uint8_t len) {
    if (len > MAX_PAYLOAD) len = MAX_PAYLOAD;

    for (uint8_t attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        Serial.print(F(" [TX] Tentativa "));
        Serial.print(attempt);
        Serial.print(F("/"));
        Serial.print(MAX_RETRIES);
        Serial.print(F(" (seq="));
        Serial.print(tx_seq);
        Serial.print(F(")... "));

        bool corrupt_now = corrupt_next_crc;
        corrupt_next_crc = false;
        if (corrupt_now) {
            Serial.print(F("[CRC RUIDO] "));
        }

        // Limpa resíduos e reseta parser
        while (linkSerial.available() > 0) linkSerial.read();
        resetAckParser();

        // Envia quadro
        sendRawFrame(MY_ID, PEER_ID, FRAME_DATA, d_type, tx_seq, payload, len, corrupt_now);

        // Aguarda ACK com timeout e delay para o simulador do Tinkercad alternar entre os Arduinos
        uint32_t start = millis();
        while (millis() - start < TIMEOUT_MS) {
            while (linkSerial.available() > 0) {
                uint8_t b = (uint8_t)linkSerial.read();
                if (checkAckByte(b, tx_seq)) {
                    Serial.println(F("<- ACK recebido!"));
                    tx_seq++;
                    return true;
                }
            }
            delay(10); // ESSENCIAL NO TINKERCAD: cede tempo para o outro Arduino rodar
        }
        Serial.println(F("[TIMEOUT] Nenhum ACK!"));
    }
    return false;
}

// ============================================================================
// OS 4 PROTÓTIPO OBRIGATÓRIOS EXIGIDOS PELO PROFESSOR
// ============================================================================
bool sendByte(uint8_t value) {
    return sendPacket(TYPE_BYTE, &value, 1);
}

bool sendWord(uint16_t value) {
    uint8_t buf[2];
    buf[0] = (uint8_t)(value & 0xFF);
    buf[1] = (uint8_t)((value >> 8) & 0xFF);
    return sendPacket(TYPE_WORD, buf, 2);
}

bool sendFloat(float value) {
    uint8_t* p = (uint8_t*)&value;
    uint8_t buf[4];
    buf[0] = p[0];
    buf[1] = p[1];
    buf[2] = p[2];
    buf[3] = p[3];
    return sendPacket(TYPE_FLOAT, buf, 4);
}

bool sendData(const uint8_t* data, uint16_t size) {
    if (size == 0) return sendPacket(TYPE_DATA, NULL, 0);
    if (size <= MAX_PAYLOAD) return sendPacket(TYPE_DATA, data, (uint8_t)size);

    uint16_t sent = 0;
    uint8_t frag = 1;
    uint8_t total_frags = (size + MAX_PAYLOAD - 1) / MAX_PAYLOAD;

    while (sent < size) {
        uint8_t chunk = (size - sent > MAX_PAYLOAD) ? MAX_PAYLOAD : (uint8_t)(size - sent);
        Serial.print(F("\n[FRAGMENTO "));
        Serial.print(frag);
        Serial.print(F("/"));
        Serial.print(total_frags);
        Serial.print(F(" ("));
        Serial.print(chunk);
        Serial.println(F(" bytes)]"));

        if (!sendPacket(TYPE_DATA, data + sent, chunk)) return false;
        sent += chunk;
        frag++;
        delay(60); // Aguarda o receptor processar e imprimir antes do próximo fragmento
    }
    return true;
}

// ============================================================================
// INTERFACE SERIAL E CONTROLE
// ============================================================================
void printMenu() {
    Serial.println(F("\n========================================"));
    Serial.println(F("       PeerDuino - Transmissor"));
    Serial.println(F("========================================"));
    Serial.println(F("Digite o comando no monitor e clique Enviar:"));
    Serial.println(F(" [1] Enviar Byte (0x42 / 66)"));
    Serial.println(F(" [2] Enviar Word (12345)"));
    Serial.println(F(" [3] Enviar Float (3.14159)"));
    Serial.println(F(" [4] Enviar String (\"Ola, PeerDuino!\")"));
    Serial.println(F(" [5] Injetar Ruido (Corrompe CRC do proximo)"));
    Serial.println(F(" [6] Enviar Bloco Longo (Fragmentado)"));
    Serial.println(F("========================================"));
}

void setup() {
    Serial.begin(9600);
    linkSerial.begin(9600);
    linkSerial.listen();

    pinMode(13, OUTPUT);
    // Pisca 1 vez longa para confirmar que o Transmissor ligou!
    digitalWrite(13, HIGH); delay(300);
    digitalWrite(13, LOW); delay(100);

    printMenu();
}

void loop() {
    if (Serial.available() > 0) {
        char cmd = (char)Serial.read();

        // Ignora quebras de linha (\r, \n), espaços e caracteres não-imprimíveis
        if (cmd <= ' ' || cmd > '~') {
            return;
        }

        // Limpa eventuais caracteres de quebra de linha restantes no buffer
        while (Serial.available() > 0) {
            char extra = (char)Serial.peek();
            if (extra <= ' ') {
                Serial.read();
            } else {
                break;
            }
        }

        switch (cmd) {
            case '1': {
                Serial.println(F("-> Enviando BYTE (0x42)"));
                if (sendByte(0x42)) {
                    Serial.println(F(">> [SUCESSO] Confirmado!"));
                } else {
                    Serial.println(F(">> [FALHA] Sem confirmacao!"));
                }
                break;
            }
            case '2': {
                Serial.println(F("-> Enviando WORD (12345)"));
                if (sendWord(12345)) {
                    Serial.println(F(">> [SUCESSO] Confirmado!"));
                } else {
                    Serial.println(F(">> [FALHA] Sem confirmacao!"));
                }
                break;
            }
            case '3': {
                Serial.println(F("-> Enviando FLOAT (3.14159)"));
                if (sendFloat(3.14159f)) {
                    Serial.println(F(">> [SUCESSO] Confirmado!"));
                } else {
                    Serial.println(F(">> [FALHA] Sem confirmacao!"));
                }
                break;
            }
            case '4': {
                const char* msg = "Ola, PeerDuino!";
                Serial.print(F("-> Enviando STRING: \""));
                Serial.print(msg);
                Serial.println(F("\""));
                if (sendData((const uint8_t*)msg, strlen(msg))) {
                    Serial.println(F(">> [SUCESSO] Confirmado!"));
                } else {
                    Serial.println(F(">> [FALHA] Sem confirmacao!"));
                }
                break;
            }
            case '5': {
                corrupt_next_crc = true;
                Serial.println(F("[TESTE] Proximo envio com CRC corrompido!"));
                Serial.println(F("        Envie [1, 2, 3 ou 4] para testar"));
                Serial.println(F("        a retransmissao automatica."));
                break;
            }
            case '6': {
                Serial.println(F("-> Enviando bloco longo (80 bytes)..."));
                uint8_t buf[80];
                for (int i = 0; i < 80; i++) buf[i] = 'A' + (i % 26);
                if (sendData(buf, 80)) {
                    Serial.println(F(">> [SUCESSO] Todos os fragmentos confirmados!"));
                } else {
                    Serial.println(F(">> [FALHA] Erro em fragmento!"));
                }
                break;
            }
            case 'm':
            case 'M':
            case '?': {
                printMenu();
                break;
            }
            default:
                // NÃO reimprime o menu para evitar loop infinito na tela
                break;
        }
    }
    delay(20); // Evita sobrecarga da CPU no simulador do Tinkercad
}
