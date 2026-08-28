#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "motor/serial_message.h"
#include "motor/serial_packet.h"
#include "motor/serial_session.h"

static MotorSerialPacket decode(const uint8_t encoded[MOTOR_SERIAL_PACKET_SIZE]) {
    MotorSerialPacket packet;
    assert(motor_serial_packet_decode(encoded, &packet) == MOTOR_SERIAL_PACKET_VALID);
    return packet;
}

static void encode(uint8_t type_flags, uint8_t sequence, const uint8_t *payload,
                   uint8_t payload_length, uint8_t encoded[MOTOR_SERIAL_PACKET_SIZE]) {
    assert(motor_serial_packet_encode(type_flags, sequence, payload, payload_length, encoded));
}

static void test_transmits_fragmented_message(void) {
    MotorSerialSession session;
    uint8_t message[58];
    uint8_t encoded[MOTOR_SERIAL_PACKET_SIZE];
    for (uint8_t index = 0; index < sizeof(message); index++) {
        message[index] = index;
    }
    motor_serial_session_init(&session);

    assert(motor_serial_session_queue(&session, 4, message, sizeof(message)));
    assert(motor_serial_session_next_packet(&session, encoded));
    MotorSerialPacket first = decode(encoded);
    assert(first.type_flags == (4 | MOTOR_SERIAL_PACKET_FIRST_FRAGMENT));
    assert(first.sequence == 0);
    assert(!motor_serial_session_next_packet(&session, encoded));

    static const uint8_t acknowledgement = 1;
    encode(1, 1, &acknowledgement, 1, encoded);
    assert(motor_serial_session_accept(&session, encoded) == MOTOR_SERIAL_SESSION_ACCEPTED);
    assert(session.sequence == 1);
    assert(session.transmit_offset == MOTOR_SERIAL_PACKET_MAX_PAYLOAD_SIZE);

    assert(motor_serial_session_next_packet(&session, encoded));
    MotorSerialPacket final = decode(encoded);
    assert(final.type_flags == (4 | MOTOR_SERIAL_PACKET_FINAL_FRAGMENT));
    assert(final.sequence == 1);
    assert(final.payload_length == 1);
    assert(final.payload[0] == message[57]);
}

static void test_receives_fragmented_message(void) {
    MotorSerialSession session;
    uint8_t message[58];
    uint8_t encoded[MOTOR_SERIAL_PACKET_SIZE];
    for (uint8_t index = 0; index < sizeof(message); index++) {
        message[index] = index;
    }
    motor_serial_session_init(&session);

    encode(4 | MOTOR_SERIAL_PACKET_FIRST_FRAGMENT, 0, message, MOTOR_SERIAL_PACKET_MAX_PAYLOAD_SIZE,
           encoded);
    assert(motor_serial_session_accept(&session, encoded) == MOTOR_SERIAL_SESSION_ACCEPTED);
    assert(session.sequence == 1);
    assert(motor_serial_session_message(&session) == 0);
    assert(motor_serial_session_next_packet(&session, encoded));
    MotorSerialPacket acknowledgement = decode(encoded);
    assert(acknowledgement.type_flags == 1);
    assert(acknowledgement.sequence == 1);
    assert(acknowledgement.payload_length == 1);
    assert(acknowledgement.payload[0] == 1);

    encode(4 | MOTOR_SERIAL_PACKET_FINAL_FRAGMENT, 1, message + 57, 1, encoded);
    assert(motor_serial_session_accept(&session, encoded) == MOTOR_SERIAL_SESSION_MESSAGE_COMPLETE);
    assert(session.sequence == 2);
    const MotorSerialMessageAssembly *received = motor_serial_session_message(&session);
    assert(received != 0);
    assert(received->type == 4);
    assert(received->length == sizeof(message));
    assert(memcmp(received->data, message, sizeof(message)) == 0);
    motor_serial_session_consume_message(&session);
    assert(motor_serial_session_message(&session) == 0);
}

static void test_resynchronizes_or_retries_from_type_zero(void) {
    MotorSerialSession session;
    static const uint8_t message[] = {0xaa};
    uint8_t encoded[MOTOR_SERIAL_PACKET_SIZE];
    motor_serial_session_init(&session);
    session.sequence = 2;
    assert(motor_serial_session_queue(&session, 4, message, sizeof(message)));
    assert(motor_serial_session_next_packet(&session, encoded));

    static const uint8_t current_type = 4;
    encode(0, 1, &current_type, 1, encoded);
    assert(motor_serial_session_accept(&session, encoded) == MOTOR_SERIAL_SESSION_ACCEPTED);
    assert(motor_serial_session_next_packet(&session, encoded));
    MotorSerialPacket resynchronization = decode(encoded);
    assert(resynchronization.type_flags == 0);
    assert(resynchronization.sequence == 2);
    assert(resynchronization.payload[0] == 4);

    encode(0, 2, &current_type, 1, encoded);
    assert(motor_serial_session_accept(&session, encoded) == MOTOR_SERIAL_SESSION_ACCEPTED);
    assert(motor_serial_session_next_packet(&session, encoded));
    MotorSerialPacket retry = decode(encoded);
    assert(retry.type_flags == 4);
    assert(retry.sequence == 2);
    assert(retry.payload[0] == message[0]);
}

static void test_requests_resynchronization_for_invalid_packet(void) {
    MotorSerialSession session;
    uint8_t encoded[MOTOR_SERIAL_PACKET_SIZE];
    motor_serial_session_init(&session);
    encode(4, 0, 0, 0, encoded);
    encoded[0] = 0;

    assert(motor_serial_session_accept(&session, encoded) == MOTOR_SERIAL_SESSION_INVALID_PACKET);
    assert(motor_serial_session_next_packet(&session, encoded));
    MotorSerialPacket resynchronization = decode(encoded);
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
