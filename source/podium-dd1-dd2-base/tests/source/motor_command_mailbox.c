#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "motor/command_mailbox.h"

static void assert_request(CommandTransport *transport, const uint8_t *expected,
                           uint16_t expected_length) {
    const uint8_t *request;
    uint16_t length;
    assert(command_transport_request(transport, &request, &length));
    assert(length == expected_length);
    assert(memcmp(request, expected, length) == 0);
}

static void test_queues_payload_read(void) {
    CommandTransport transport;
    uint8_t payload[16];
    command_transport_init(&transport);

    assert(motor_command_mailbox_queue_payload_read(&transport, payload, sizeof(payload)) ==
           COMMAND_TRANSPORT_COMPLETE);
    static const uint8_t expected[] = {2, 0x41, 0x80, 0x10, 0};
    assert_request(&transport, expected, sizeof(expected));
}

static void test_queues_payload_write(void) {
    CommandTransport transport;
    static const uint8_t payload[] = {0xaa, 0xbb, 0xcc};
    command_transport_init(&transport);

    assert(motor_command_mailbox_queue_payload_write(&transport, payload, sizeof(payload)) ==
           COMMAND_TRANSPORT_COMPLETE);
    static const uint8_t expected[] = {2, 0x40, 0x80, 0xaa, 0xbb, 0xcc};
    assert_request(&transport, expected, sizeof(expected));
}

static void test_queues_length_read(void) {
    CommandTransport transport;
    uint8_t length[MOTOR_COMMAND_MAILBOX_LENGTH_SIZE];
    command_transport_init(&transport);

    assert(motor_command_mailbox_queue_length_read(&transport, length) ==
           COMMAND_TRANSPORT_COMPLETE);
    static const uint8_t expected[] = {2, 0x41, 0x81, 2, 0};
    assert_request(&transport, expected, sizeof(expected));
}

static void test_queues_control_read(void) {
    CommandTransport transport;
    uint8_t control[MOTOR_COMMAND_MAILBOX_CONTROL_SIZE];
    command_transport_init(&transport);

    assert(motor_command_mailbox_queue_control_read(&transport, control) ==
           COMMAND_TRANSPORT_COMPLETE);
    static const uint8_t expected[] = {2, 0x41, 0x82, 4, 0};
    assert_request(&transport, expected, sizeof(expected));
}

static void test_queues_status_read(void) {
    CommandTransport transport;
    uint8_t status[MOTOR_COMMAND_MAILBOX_STATUS_SIZE];
    command_transport_init(&transport);

    assert(motor_command_mailbox_queue_status_read(&transport, status) ==
           COMMAND_TRANSPORT_COMPLETE);
    static const uint8_t expected[] = {2, 0x41, 0x90, 4, 0};
    assert_request(&transport, expected, sizeof(expected));
}

static void test_waits_for_active_transport(void) {
    CommandTransport transport;
    uint8_t control[MOTOR_COMMAND_MAILBOX_CONTROL_SIZE];
    command_transport_init(&transport);

    assert(motor_command_mailbox_queue_control_read(&transport, control) ==
           COMMAND_TRANSPORT_COMPLETE);
    assert(motor_command_mailbox_queue_status_read(&transport, control) == COMMAND_TRANSPORT_BUSY);
}

static void test_decodes_control(void) {
    static const uint8_t record[] = {0x40, 0xa5, 0x01, 0xf4};
    MotorCommandMailboxControl control;

    assert(motor_command_mailbox_control_decode(record, &control));
    assert(control.flags == 0x40);
    assert(control.reserved == 0xa5);
    assert(control.payload_length == 500);
    assert(!motor_command_mailbox_control_decode(0, &control));
    assert(!motor_command_mailbox_control_decode(record, 0));
}

int main(void) {
    test_queues_payload_read();
    test_queues_payload_write();
    test_queues_length_read();
    test_queues_control_read();
    test_queues_status_read();
    test_waits_for_active_transport();
    test_decodes_control();
    return 0;
}
