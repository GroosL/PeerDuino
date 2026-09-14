#include "frame.hpp"
#include "error.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>

void test_crc16() {
    printf("[TEST] Testing CRC-16...\n");
    const uint8_t data[] = "123456789";
    uint16_t c1 = crc16(data, 9);
    uint16_t c2 = 0;
    for (int i = 0; i < 9; i++) {
        c2 = crc16_update(c2, data[i]);
    }
    assert(c1 == c2);
    assert(verify_crc(data, 9, c1) == true);
    assert(verify_crc(data, 9, c1 ^ 1) == false);
    printf("  -> OK (CRC: 0x%04X)\n", c1);
}

void test_serialize_deserialize() {
    printf("[TEST] Testing serialize & deserialize...\n");
    Frame tx{};
    tx.src = 1;
    tx.dst = 2;
    tx.frame_type = FRAME_DATA;
    tx.data_type = TYPE_WORD;
    tx.seq = 42;
    tx.length = 2;
    tx.payload[0] = 0x34;
    tx.payload[1] = 0x12;

    uint8_t wire[150];
    size_t wire_len = serialize(tx, wire);
    assert(wire_len > 0);
    assert(wire[0] == 0x7E);
    assert(wire[wire_len - 1] == 0x7E);

    Frame rx{};
    assert(deserialize(wire, wire_len, rx) == true);
    assert(rx.src == tx.src);
    assert(rx.dst == tx.dst);
    assert(rx.frame_type == tx.frame_type);
    assert(rx.data_type == tx.data_type);
    assert(rx.seq == tx.seq);
    assert(rx.length == tx.length);
    assert(rx.payload[0] == 0x34 && rx.payload[1] == 0x12);
    printf("  -> OK\n");
}

void test_byte_stuffing() {
    printf("[TEST] Testing Byte Stuffing (0x7E e 0x7D)...\n");
    Frame tx{};
    tx.src = 1;
    tx.dst = 2;
    tx.frame_type = FRAME_DATA;
    tx.data_type = TYPE_DATA;
    tx.seq = 7;
    tx.length = 4;
    tx.payload[0] = 0x7E;
    tx.payload[1] = 0x7D;
    tx.payload[2] = 0x5E;
    tx.payload[3] = 0x5D;

    uint8_t wire[150];
    size_t wire_len = serialize(tx, wire);
    assert(wire_len > 0);

    // No interior 0x7E allowed
    for (size_t i = 1; i < wire_len - 1; i++) {
        assert(wire[i] != 0x7E);
    }

    Frame rx{};
    assert(deserialize(wire, wire_len, rx) == true);
    assert(rx.length == 4);
    assert(rx.payload[0] == 0x7E);
    assert(rx.payload[1] == 0x7D);
    assert(rx.payload[2] == 0x5E);
    assert(rx.payload[3] == 0x5D);
    printf("  -> OK\n");
}

void test_incremental_parser() {
    printf("[TEST] Testing FrameParser byte-by-byte with noise...\n");
    Frame tx{};
    tx.src = 1;
    tx.dst = 2;
    tx.frame_type = FRAME_ACK;
    tx.data_type = TYPE_NONE;
    tx.seq = 99;
    tx.length = 0;

    uint8_t wire[150];
    size_t wire_len = serialize(tx, wire);

    FrameParser parser;
    Frame rx{};

    // Feed noise first
    assert(parser.feed(0x00, rx) == false);
    assert(parser.feed(0xFF, rx) == false);
    assert(parser.feed(0x7E, rx) == false); // first start flag

    bool completed = false;
    for (size_t i = 1; i < wire_len; i++) {
        if (parser.feed(wire[i], rx)) {
            completed = true;
        }
    }
    assert(completed == true);
    assert(rx.seq == 99);
    assert(rx.frame_type == FRAME_ACK);
    printf("  -> OK\n");
}

int main() {
    printf("--- Running tests ---\n");
    test_crc16();
    test_serialize_deserialize();
    test_byte_stuffing();
    test_incremental_parser();
    printf("--- All tests passed! ---\n");
    return 0;
}

