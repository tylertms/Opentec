#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "serial/message.h"
#include "serial/packet.h"
#include "serial/session.h"

static SerialPacket decode(const uint8_t encoded[SERIAL_PACKET_SIZE]) {
    SerialPacket packet;
    assert(serial_packet_decode(encoded, &packet) == SERIAL_PACKET_VALID);
    return packet;
}

static void encode(uint8_t type_flags, uint8_t sequence, const uint8_t *payload,
                   uint8_t payload_length, uint8_t encoded[SERIAL_PACKET_SIZE]) {
    assert(serial_packet_encode(type_flags, sequence, payload, payload_length, encoded));
}

static void test_transmits_fragmented_message(void) {
    SerialSession session;
    uint8_t message[58];
    uint8_t encoded[SERIAL_PACKET_SIZE];
    for (uint8_t index = 0; index < sizeof(message); index++) {
        message[index] = index;
    }
    serial_session_init(&session);

    assert(serial_session_queue(&session, 4, message, sizeof(message)));
    assert(serial_session_next_packet(&session, encoded));
    SerialPacket first = decode(encoded);
    assert(first.type_flags == (4 | SERIAL_PACKET_FIRST_FRAGMENT));
    assert(first.sequence == 0);
    assert(!serial_session_next_packet(&session, encoded));

    static const uint8_t acknowledgement = 1;
    encode(1, 1, &acknowledgement, 1, encoded);
    assert(serial_session_accept(&session, encoded) == SERIAL_SESSION_ACCEPTED);
    assert(session.sequence == 1);
    assert(session.transmit_offset == SERIAL_PACKET_MAX_PAYLOAD_SIZE);

    assert(serial_session_next_packet(&session, encoded));
    SerialPacket final = decode(encoded);
    assert(final.type_flags == (4 | SERIAL_PACKET_FINAL_FRAGMENT));
    assert(final.sequence == 1);
    assert(final.payload_length == 1);
    assert(final.payload[0] == message[57]);
}

static void test_receives_fragmented_message(void) {
    SerialSession session;
    uint8_t message[58];
    uint8_t encoded[SERIAL_PACKET_SIZE];
    for (uint8_t index = 0; index < sizeof(message); index++) {
        message[index] = index;
    }
    serial_session_init(&session);

    encode(4 | SERIAL_PACKET_FIRST_FRAGMENT, 0, message, SERIAL_PACKET_MAX_PAYLOAD_SIZE, encoded);
    assert(serial_session_accept(&session, encoded) == SERIAL_SESSION_ACCEPTED);
    assert(session.sequence == 1);
    assert(serial_session_message(&session) == 0);
    assert(serial_session_next_packet(&session, encoded));
    SerialPacket acknowledgement = decode(encoded);
    assert(acknowledgement.type_flags == 1);
    assert(acknowledgement.sequence == 1);
    assert(acknowledgement.payload_length == 1);
    assert(acknowledgement.payload[0] == 1);

    encode(4 | SERIAL_PACKET_FINAL_FRAGMENT, 1, message + 57, 1, encoded);
    assert(serial_session_accept(&session, encoded) == SERIAL_SESSION_MESSAGE_COMPLETE);
    assert(session.sequence == 2);
    const SerialMessageAssembly *received = serial_session_message(&session);
    assert(received != 0);
    assert(received->type == 4);
    assert(received->length == sizeof(message));
    assert(memcmp(received->data, message, sizeof(message)) == 0);
    serial_session_consume_message(&session);
    assert(serial_session_message(&session) == 0);
}

static void test_resynchronizes_or_retries_from_type_zero(void) {
    SerialSession session;
    static const uint8_t message[] = {0xaa};
    uint8_t encoded[SERIAL_PACKET_SIZE];
    serial_session_init(&session);
    session.sequence = 2;
    assert(serial_session_queue(&session, 4, message, sizeof(message)));
    assert(serial_session_next_packet(&session, encoded));

    static const uint8_t current_type = 4;
    encode(0, 1, &current_type, 1, encoded);
    assert(serial_session_accept(&session, encoded) == SERIAL_SESSION_ACCEPTED);
    assert(serial_session_next_packet(&session, encoded));
    SerialPacket resynchronization = decode(encoded);
    assert(resynchronization.type_flags == 0);
    assert(resynchronization.sequence == 2);
    assert(resynchronization.payload[0] == 4);

    encode(0, 2, &current_type, 1, encoded);
    assert(serial_session_accept(&session, encoded) == SERIAL_SESSION_ACCEPTED);
    assert(serial_session_next_packet(&session, encoded));
    SerialPacket retry = decode(encoded);
    assert(retry.type_flags == 4);
    assert(retry.sequence == 2);
    assert(retry.payload[0] == message[0]);
}

static void test_requests_resynchronization_for_invalid_packet(void) {
    SerialSession session;
    uint8_t encoded[SERIAL_PACKET_SIZE];
    serial_session_init(&session);
    encode(4, 0, 0, 0, encoded);
    encoded[0] = 0;

    assert(serial_session_accept(&session, encoded) == SERIAL_SESSION_INVALID_PACKET);
    assert(serial_session_next_packet(&session, encoded));
    SerialPacket resynchronization = decode(encoded);
    assert(resynchronization.type_flags == 0);
    assert(resynchronization.sequence == 0);
    assert(resynchronization.payload[0] == 0xff);
}

int main(void) {
    test_transmits_fragmented_message();
    test_receives_fragmented_message();
    test_resynchronizes_or_retries_from_type_zero();
    test_requests_resynchronization_for_invalid_packet();
    return 0;
}
