#include "arq.hpp"
#include <cassert>
#include <cstdio>
#include <queue>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include <cmath>

class ThreadSafeSerial : public Stream {
public:
    std::mutex mtx;
    std::queue<uint8_t> q;

    int read() override {
        std::lock_guard<std::mutex> lock(mtx);
        if (q.empty()) return -1;
        uint8_t b = q.front();
        q.pop();
        return b;
    }

    int available() override {
        std::lock_guard<std::mutex> lock(mtx);
        return static_cast<int>(q.size());
    }

    size_t write(uint8_t b) override {
        std::lock_guard<std::mutex> lock(mtx);
        q.push(b);
        return 1;
    }
};

struct DualLink {
    ThreadSafeSerial a_to_b;
    ThreadSafeSerial b_to_a;

    struct StreamA : public Stream {
        DualLink& l;
        StreamA(DualLink& link) : l(link) {}
        int read() override { return l.b_to_a.read(); }
        int available() override { return l.b_to_a.available(); }
        size_t write(uint8_t b) override { return l.a_to_b.write(b); }
    } streamA{*this};

    struct StreamB : public Stream {
        DualLink& l;
        StreamB(DualLink& link) : l(link) {}
        int read() override { return l.a_to_b.read(); }
        int available() override { return l.a_to_b.available(); }
        size_t write(uint8_t b) override { return l.b_to_a.write(b); }
    } streamB{*this};
};

int main() {
    printf("========================================\n");
    printf("     PeerDuino ARQ Emulation Tests      \n");
    printf("========================================\n");

    DualLink link;
    ARQ nodeA(link.streamA, 1, 2);
    ARQ nodeB(link.streamB, 2, 1);

    nodeA.setTimeout(200);
    nodeB.setTimeout(200);

    std::atomic<bool> running{true};
    std::queue<Frame> received_frames;
    std::mutex rx_mtx;

    // Thread do Arduino B (Receptor): fica em loop() chamando receive()
    std::thread threadB([&]() {
        while (running) {
            Frame rx;
            if (nodeB.receive(rx)) {
                std::lock_guard<std::mutex> lock(rx_mtx);
                received_frames.push(rx);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });

    // 1. Teste de sendByte
    printf("[TEST] sendByte(0x42)...\n");
    bool ok_byte = nodeA.sendByte(0x42);
    assert(ok_byte == true);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    {
        std::lock_guard<std::mutex> lock(rx_mtx);
        assert(!received_frames.empty());
        Frame f = received_frames.front();
        received_frames.pop();
        assert(f.data_type == TYPE_BYTE);
        assert(ARQ::getByte(f) == 0x42);
    }
    printf("  -> OK\n");

    // 2. Teste de sendWord
    printf("[TEST] sendWord(12345)...\n");
    bool ok_word = nodeA.sendWord(12345);
    assert(ok_word == true);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    {
        std::lock_guard<std::mutex> lock(rx_mtx);
        assert(!received_frames.empty());
        Frame f = received_frames.front();
        received_frames.pop();
        assert(f.data_type == TYPE_WORD);
        assert(ARQ::getWord(f) == 12345);
    }
    printf("  -> OK\n");

    // 3. Teste de sendFloat
    printf("[TEST] sendFloat(3.14159f)...\n");
    bool ok_float = nodeA.sendFloat(3.14159f);
    assert(ok_float == true);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    {
        std::lock_guard<std::mutex> lock(rx_mtx);
        assert(!received_frames.empty());
        Frame f = received_frames.front();
        received_frames.pop();
        assert(f.data_type == TYPE_FLOAT);
        assert(std::fabs(ARQ::getFloat(f) - 3.14159f) < 0.0001f);
    }
    printf("  -> OK\n");

    // 4. Teste de sendData fragmentado (100 bytes > MAX_PAYLOAD)
    printf("[TEST] sendData(100 bytes fragmented)...\n");
    uint8_t big_payload[100];
    for (int i = 0; i < 100; i++) big_payload[i] = static_cast<uint8_t>(i + 1);
    bool ok_data = nodeA.sendData(big_payload, 100);
    assert(ok_data == true);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    {
        std::lock_guard<std::mutex> lock(rx_mtx);
        // Deve ter recebido 2 pacotes: 64 bytes e 36 bytes
        assert(received_frames.size() == 2);
        Frame f1 = received_frames.front(); received_frames.pop();
        Frame f2 = received_frames.front(); received_frames.pop();
        assert(f1.length == 64);
        assert(f2.length == 36);
        assert(f1.payload[0] == 1);
        assert(f2.payload[0] == 65);
    }
    printf("  -> OK\n");

    // 5. Teste de injeção de erro de CRC (retransmissão automática e recuperação)
    printf("[TEST] Corrupted CRC injection & automatic retry recovery...\n");
    nodeA.corrupt_next_crc = true;
    bool ok_corrupt = nodeA.sendByte(0x99);
    assert(ok_corrupt == true); // Transmissor tentou, sofreu timeout, retransmitiu e conseguiu!
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    {
        std::lock_guard<std::mutex> lock(rx_mtx);
        assert(!received_frames.empty());
        Frame f = received_frames.front();
        received_frames.pop();
        assert(ARQ::getByte(f) == 0x99);
    }
    printf("  -> OK (recovered on retry!)\n");

    // 6. Teste de perda de ACK (receptor descarta ACK, transmissor retransmite, receptor não duplica)
    printf("[TEST] Dropped ACK simulation & duplicate filtering...\n");
    nodeB.drop_next_ack = true;
    bool ok_ack_drop = nodeA.sendByte(0x77);
    assert(ok_ack_drop == true);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    {
        std::lock_guard<std::mutex> lock(rx_mtx);
        // Deve ter chegado APENAS 1 vez para a aplicação (duplicata filtrada!)
        assert(received_frames.size() == 1);
        Frame f = received_frames.front();
        received_frames.pop();
        assert(ARQ::getByte(f) == 0x77);
    }
    printf("  -> OK (duplicate prevented!)\n");

    // Finaliza thread
    running = false;
    threadB.join();

    printf("========================================\n");
    printf("  ALL ARQ TESTS PASSED SUCCESSFULLY!    \n");
    printf("========================================\n");
    return 0;
}
