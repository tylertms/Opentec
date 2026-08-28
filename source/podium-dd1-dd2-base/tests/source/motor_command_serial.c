#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "motor/command_serial.h"
#include "motor/serial_packet.h"
#include "motor/serial_session.h"
#include "transfer/command.h"

static void receive(MotorSerialSession *session, const uint8_t *payload, uint8_t length) {
    uint8_t encoded[MOTOR_SERIAL_PACKET_SIZE];
    assert(motor_serial_packet_encode(4, 1, payload, length, encoded));
    assert(motor_serial_session_accept(session, encoded) == MOTOR_SERIAL_SESSION_MESSAGE_COMPLETE);
}

static void test_submits_write_and_applies_response(void) {
    CommandTransport transport;
    MotorSerialSession session;
    static const uint8_t data[] = {0xaa, 0xbb};
    static const uint8_t expected[] = {2, 0x40, 0x90, 0xaa, 0xbb};
    uint8_t encoded[MOTOR_SERIAL_PACKET_SIZE];
    command_transport_init(&transport);
    motor_serial_session_init(&session);

    assert(command_transport_queue_write(&transport, 0x20, 0x90, data, sizeof(data)) ==
           COMMAND_TRANSPORT_COMPLETE);
    assert(motor_command_serial_submit(&transport, &session));
    assert(motor_serial_session_next_packet(&session, encoded));
    MotorSerialPacket packet;
    assert(motor_serial_packet_decode(encoded, &packet) == MOTOR_SERIAL_PACKET_VALID);
    assert(packet.type_flags == 4);
    assert(packet.sequence == 0);
    assert(packet.payload_length == sizeof(expected));
    assert(memcmp(packet.payload, expected, sizeof(expected)) == 0);

    static const uint8_t accepted[] = {1};
    receive(&session, accepted, sizeof(accepted));
    assert(motor_command_serial_receive(&transport, &session));
    assert(command_transport_poll(&transport, 0x20) == COMMAND_TRANSPORT_COMPLETE);
    assert(!session.transmit_active);
    assert(motor_serial_session_message(&session) == 0);
}

static void test_submits_read_and_copies_response(void) {
    CommandTransport transport;
    MotorSerialSession session;
    uint8_t output[3] = {0};
    command_transport_init(&transport);
    motor_serial_session_init(&session);

    assert(command_transport_queue_read(&transport, 0x20, 0x80, output, sizeof(output)) ==
           COMMAND_TRANSPORT_COMPLETE);
    assert(motor_command_serial_submit(&transport, &session));
    static const uint8_t response[] = {1, 0, 0x11, 0x22, 0x33};
    receive(&session, response, sizeof(response));
    assert(motor_command_serial_receive(&transport, &session));
    assert(memcmp(output, response + 2, sizeof(output)) == 0);
}

static void test_ignores_other_message_types(void) {
    CommandTransport transport;
    MotorSerialSession session;
    uint8_t encoded[MOTOR_SERIAL_PACKET_SIZE];
    static const uint8_t payload[] = {1};
    command_transport_init(&transport);
    motor_serial_session_init(&session);
    assert(motor_serial_packet_encode(2, 0, payload, sizeof(payload), encoded));
    assert(motor_serial_session_accept(&session, encoded) == MOTOR_SERIAL_SESSION_MESSAGE_COMPLETE);

    assert(!motor_command_serial_receive(&transport, &session));
    assert(motor_serial_session_message(&session) != 0);
}

int main(void) {
    test_submits_write_and_applies_response();
    test_submits_read_and_copies_response();
    test_ignores_other_message_types();
    return 0;
}
