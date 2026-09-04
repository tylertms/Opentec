#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "serial/message.h"

static void fill_message(uint8_t *message, uint16_t length) {
    for (uint16_t index = 0; index < length; index++) {
        message[index] = (uint8_t)index;
    }
}

static SerialPacket encode_and_decode(const uint8_t *message, uint16_t length, uint16_t offset,
                                      uint16_t *next_offset, bool *acknowledgement_required) {
    uint8_t encoded[SERIAL_PACKET_SIZE];
    SerialPacket packet;
    assert(serial_message_fragment_encode(4, 0x2a, message, length, offset, encoded, next_offset,
                                          acknowledgement_required));
    assert(serial_packet_decode(encoded, &packet) == SERIAL_PACKET_VALID);
    return packet;
}

static void test_encodes_single_packet_message(void) {
    uint8_t message[SERIAL_PACKET_MAX_PAYLOAD_SIZE];
    uint16_t next_offset;
    bool acknowledgement_required;
    fill_message(message, sizeof(message));

    SerialPacket packet =
        encode_and_decode(message, sizeof(message), 0, &next_offset, &acknowledgement_required);

    assert(packet.type_flags == 4);
    assert(packet.sequence == 0x2a);
    assert(packet.payload_length == sizeof(message));
    assert(memcmp(packet.payload, message, sizeof(message)) == 0);
    assert(next_offset == sizeof(message));
    assert(!acknowledgement_required);
}

static void test_encodes_first_and_final_fragments(void) {
    uint8_t message[58];
    uint16_t next_offset;
    bool acknowledgement_required;
    fill_message(message, sizeof(message));

    SerialPacket first =
        encode_and_decode(message, sizeof(message), 0, &next_offset, &acknowledgement_required);
    assert(first.type_flags == (4 | SERIAL_PACKET_FIRST_FRAGMENT));
    assert(first.payload_length == SERIAL_PACKET_MAX_PAYLOAD_SIZE);
    assert(next_offset == SERIAL_PACKET_MAX_PAYLOAD_SIZE);
    assert(acknowledgement_required);

    SerialPacket final = encode_and_decode(message, sizeof(message), next_offset, &next_offset,
                                           &acknowledgement_required);
    assert(final.type_flags == (4 | SERIAL_PACKET_FINAL_FRAGMENT));
    assert(final.payload_length == 1);
    assert(final.payload[0] == message[57]);
    assert(next_offset == sizeof(message));
    assert(!acknowledgement_required);
}

static void test_encodes_and_assembles_maximum_message(void) {
    assert(SERIAL_MESSAGE_MAX_SIZE == 512);
    uint8_t message[SERIAL_MESSAGE_MAX_SIZE];
    SerialMessageAssembly assembly;
    uint16_t offset = 0;
    uint8_t packet_count = 0;
    fill_message(message, sizeof(message));
    serial_message_assembly_reset(&assembly);

    while (offset < sizeof(message)) {
        uint16_t next_offset;
        bool acknowledgement_required;
        SerialPacket packet = encode_and_decode(message, sizeof(message), offset, &next_offset,
                                                &acknowledgement_required);
        SerialMessageResult result = serial_message_accept(&assembly, &packet);
        packet_count++;
        if (next_offset == sizeof(message)) {
            assert(packet.type_flags == (4 | SERIAL_PACKET_FINAL_FRAGMENT));
            assert(packet.payload_length == 56);
            assert(result == SERIAL_MESSAGE_COMPLETE);
            assert(!acknowledgement_required);
        } else {
            assert(result == SERIAL_MESSAGE_ACKNOWLEDGE);
            assert(acknowledgement_required);
            assert((packet.type_flags & SERIAL_PACKET_FIRST_FRAGMENT) != 0 ||
                   (packet.type_flags & SERIAL_PACKET_CONTINUATION_FRAGMENT) != 0);
        }
        offset = next_offset;
    }

    assert(packet_count == 9);
    assert(assembly.type == 4);
    assert(assembly.length == sizeof(message));
    assert(memcmp(assembly.data, message, sizeof(message)) == 0);
}

static void test_encodes_acknowledgement(void) {
    uint8_t encoded[SERIAL_PACKET_SIZE];
    SerialPacket packet;

    assert(serial_message_acknowledgement_encode(0x31, 4, encoded));
    assert(serial_packet_decode(encoded, &packet) == SERIAL_PACKET_VALID);
    assert(packet.type_flags == 1);
    assert(packet.sequence == 0x31);
    assert(packet.payload_length == 1);
    assert(packet.payload[0] == 4);
}

static void test_rejects_invalid_messages(void) {
    uint8_t message[SERIAL_MESSAGE_MAX_SIZE + 1] = {0};
    uint8_t encoded[SERIAL_PACKET_SIZE];
    uint16_t next_offset;
    bool acknowledgement_required;
    SerialMessageAssembly assembly;
    SerialPacket packet = {.type_flags = 1, .payload_length = 1};
    serial_message_assembly_reset(&assembly);

    assert(!serial_message_fragment_encode(1, 0, message, 1, 0, encoded, &next_offset,
                                           &acknowledgement_required));
    assert(!serial_message_fragment_encode(4, 0, message, sizeof(message), 0, encoded, &next_offset,
                                           &acknowledgement_required));
    assert(serial_message_accept(&assembly, &packet) == SERIAL_MESSAGE_INVALID_PACKET);
    assembly.length = SERIAL_MESSAGE_MAX_SIZE;
    assembly.type = 4;
    packet.type_flags = 4;
    assert(serial_message_accept(&assembly, &packet) == SERIAL_MESSAGE_OVERFLOW);
}

static void test_clears_assembly_after_overflow(void) {
    SerialMessageAssembly assembly;
    SerialPacket packet = {
        .type_flags = 4,
        .payload = {0x11, 0x22},
        .payload_length = 2,
    };
    serial_message_assembly_reset(&assembly);
    assembly.length = SERIAL_MESSAGE_MAX_SIZE - 1;
    assembly.type = 4;

    assert(serial_message_accept(&assembly, &packet) == SERIAL_MESSAGE_OVERFLOW);
    assert(assembly.length == 0);
    assert(assembly.type == 0);
    packet.type_flags = 2;
    packet.payload_length = 1;
    assert(serial_message_accept(&assembly, &packet) == SERIAL_MESSAGE_COMPLETE);
    assert(assembly.type == 2);
    assert(assembly.length == 1);
    assert(assembly.data[0] == 0x11);
}

int main(void) {
    test_encodes_single_packet_message();
    test_encodes_first_and_final_fragments();
    test_encodes_and_assembles_maximum_message();
    test_encodes_acknowledgement();
    test_rejects_invalid_messages();
    test_clears_assembly_after_overflow();
    return 0;
}
