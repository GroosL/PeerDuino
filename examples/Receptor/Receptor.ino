#include <SoftwareSerial.h>
#include "arq.hpp"

// SoftwareSerial nos pinos 10 (RX) e 11 (TX) para enlace entre as duas placas
// Conexao: Pino 10 (RX) do Receptor <---> Pino 11 (TX) do Transmissor
//          Pino 11 (TX) do Receptor <---> Pino 10 (RX) do Transmissor
//          GND <---> GND (obrigatorio)
SoftwareSerial linkSerial(10, 11);

// ARQ(porta, meu_id, id_remoto)
ARQ node(linkSerial, 2, 1);

void setup() {
    // Porta USB do PC para exibição dos dados recebidos
    Serial.begin(115200);
    while (!Serial && millis() < 2000);

    // Enlace serial entre os dois Arduinos
    linkSerial.begin(9600);
    node.begin();

    Serial.println(F("========================================"));
    Serial.println(F("        PeerDuino - Receptor"));
    Serial.println(F("========================================"));
    Serial.println(F("Aguardando mensagens do Transmissor..."));
    Serial.println(F("Dica: envie 'd' para simular perda de ACK."));
    Serial.println(F("========================================\n"));
}

void loop() {
    // Permite simular perda de confirmação (ACK) pelo monitor serial do receptor
    if (Serial.available() > 0) {
        char c = Serial.read();
        if (c == 'd' || c == 'D') {
            node.drop_next_ack = true;
            Serial.println(F("[AVISO] O proximo ACK recebido sera DESCARTADO!"));
            Serial.println(F("        Isso fara o transmissor reenviar o quadro duplicado"));
            Serial.println(F("        e o receptor demonstrara a filtragem de duplicata.\n"));
        }
    }

    // Processa a recepção serial de forma não-bloqueante
    Frame rx;
    if (node.receive(rx)) {
        Serial.print(F("[RX RECEBIDO] Seq: "));
        Serial.print(rx.seq);
        Serial.print(F(" | Tam: "));
        Serial.print(rx.length);
        Serial.print(F(" bytes | "));

        // Identifica e decodifica os tipos obrigatórios da especificação
        switch (rx.data_type) {
            case TYPE_BYTE: {
                uint8_t val = ARQ::getByte(rx);
                Serial.print(F("Tipo: BYTE | Valor: "));
                Serial.print(val);
                Serial.print(F(" (Hex: 0x"));
                Serial.print(val, HEX);
                Serial.println(F(")"));
                break;
            }

            case TYPE_WORD: {
                uint16_t val = ARQ::getWord(rx);
                Serial.print(F("Tipo: WORD | Valor: "));
                Serial.println(val);
                break;
            }

            case TYPE_FLOAT: {
                float val = ARQ::getFloat(rx);
                Serial.print(F("Tipo: FLOAT | Valor: "));
                Serial.println(val, 5);
                break;
            }

            case TYPE_DATA: {
                Serial.print(F("Tipo: DADOS | Conteudo: \""));
                for (uint8_t i = 0; i < rx.length; i++) {
                    Serial.write(rx.payload[i]);
                }
                Serial.println(F("\""));
                break;
            }

            default:
                Serial.print(F("Tipo DESCONHECIDO ("));
                Serial.print(rx.data_type);
                Serial.println(F(")"));
                break;
        }
    }
}
