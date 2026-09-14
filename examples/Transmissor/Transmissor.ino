#include <SoftwareSerial.h>
#include "arq.hpp"

// SoftwareSerial nos pinos 10 (RX) e 11 (TX) para enlace entre as duas placas
// Conexao: Pino 10 (RX) do Transmissor <---> Pino 11 (TX) do Receptor
//          Pino 11 (TX) do Transmissor <---> Pino 10 (RX) do Receptor
//          GND <---> GND (obrigatorio)
SoftwareSerial linkSerial(10, 11);

// ARQ(porta, meu_id, id_destino)
ARQ node(linkSerial, 1, 2);

void printMenu() {
    Serial.println(F("\n========================================"));
    Serial.println(F("       PeerDuino - Transmissor"));
    Serial.println(F("========================================"));
    Serial.println(F("Escolha uma opcao pelo Serial Monitor:"));
    Serial.println(F(" [1] Enviar Byte (0x42 / 66)"));
    Serial.println(F(" [2] Enviar Word (12345)"));
    Serial.println(F(" [3] Enviar Float (3.14159)"));
    Serial.println(F(" [4] Enviar String (\"Ola, PeerDuino!\")"));
    Serial.println(F(" [5] Injetar Ruido (Corromper CRC no proximo envio)"));
    Serial.println(F(" [6] Enviar Bloco Longo (100 bytes fragmentados)"));
    Serial.println(F("========================================"));
}

void setup() {
    // Porta USB do PC para depuração e menu interativo
    Serial.begin(115200);
    while (!Serial && millis() < 2000);

    // Enlace serial entre os dois Arduinos
    linkSerial.begin(9600);
    node.begin();

    // Configura timeout de 400ms e 4 tentativas
    node.setTimeout(400);
    node.setMaxRetries(4);

    printMenu();
}

void loop() {
    if (Serial.available() > 0) {
        char cmd = Serial.read();
        if (cmd == '\r' || cmd == '\n') return;

        Serial.println();
        switch (cmd) {
            case '1': {
                uint8_t valor = 0x42;
                Serial.print(F("-> Enviando BYTE: "));
                Serial.println(valor);
                uint32_t t0 = millis();
                if (node.sendByte(valor)) {
                    Serial.print(F(">> [SUCESSO] Confirmado com ACK! (RTT: "));
                    Serial.print(millis() - t0);
                    Serial.println(F(" ms)"));
                } else {
                    Serial.println(F(">> [FALHA] Sem confirmacao apos 4 tentativas (Timeout)!"));
                }
                break;
            }

            case '2': {
                uint16_t valor = 12345;
                Serial.print(F("-> Enviando WORD: "));
                Serial.println(valor);
                uint32_t t0 = millis();
                if (node.sendWord(valor)) {
                    Serial.print(F(">> [SUCESSO] Confirmado com ACK! (RTT: "));
                    Serial.print(millis() - t0);
                    Serial.println(F(" ms)"));
                } else {
                    Serial.println(F(">> [FALHA] Sem confirmacao apos 4 tentativas (Timeout)!"));
                }
                break;
            }

            case '3': {
                float valor = 3.14159f;
                Serial.print(F("-> Enviando FLOAT: "));
                Serial.println(valor, 5);
                uint32_t t0 = millis();
                if (node.sendFloat(valor)) {
                    Serial.print(F(">> [SUCESSO] Confirmado com ACK! (RTT: "));
                    Serial.print(millis() - t0);
                    Serial.println(F(" ms)"));
                } else {
                    Serial.println(F(">> [FALHA] Sem confirmacao apos 4 tentativas (Timeout)!"));
                }
                break;
            }

            case '4': {
                const char* msg = "Ola, PeerDuino!";
                Serial.print(F("-> Enviando STRING: \""));
                Serial.print(msg);
                Serial.println(F("\""));
                uint32_t t0 = millis();
                if (node.sendData((const uint8_t*)msg, strlen(msg))) {
                    Serial.print(F(">> [SUCESSO] Confirmado com ACK! (RTT: "));
                    Serial.print(millis() - t0);
                    Serial.println(F(" ms)"));
                } else {
                    Serial.println(F(">> [FALHA] Sem confirmacao apos 4 tentativas (Timeout)!"));
                }
                break;
            }

            case '5': {
                node.corrupt_next_crc = true;
                Serial.println(F("[AVISO] O proximo quadro sera enviado com CRC CORROMPIDO!"));
                Serial.println(F("        Escolha agora uma opcao [1, 2, 3 ou 4] para testar"));
                Serial.println(F("        a retransmissao automatica por erro de integridade."));
                break;
            }

            case '6': {
                Serial.println(F("-> Gerando bloco de 100 bytes (maior que 64 bytes)..."));
                uint8_t buffer[100];
                for (int i = 0; i < 100; i++) buffer[i] = (uint8_t)('A' + (i % 26));
                uint32_t t0 = millis();
                if (node.sendData(buffer, 100)) {
                    Serial.print(F(">> [SUCESSO] Todos os fragmentos confirmados! (Tempo total: "));
                    Serial.print(millis() - t0);
                    Serial.println(F(" ms)"));
                } else {
                    Serial.println(F(">> [FALHA] Houve falha na entrega de um dos fragmentos!"));
                }
                break;
            }

            default:
                printMenu();
                break;
        }
    }
}
