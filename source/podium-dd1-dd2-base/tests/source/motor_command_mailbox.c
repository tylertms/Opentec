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

static void test_queues_payload_write(void) {
    static const uint8_t payload[] = {0x11, 0x22, 0x33};
    static const uint8_t expected[] = {0x02, 0x40, 0x80, 0x11, 0x22, 0x33};
    CommandTransport transport;
    command_transport_init(&transport);

    assert(motor_command_mailbox_queue_payload(&transport, payload, sizeof(payload)) ==
           COMMAND_TRANSPORT_COMPLETE);
    assert_request(&transport, expected, sizeof(expected));
}

static void test_queues_control_write(void) {
    static const uint8_t control[] = {0x80, 0x01, 0x02, 0x03};
    static const uint8_t expected[] = {0x02, 0x40, 0x82, 0x80, 0x01, 0x02, 0x03};
    CommandTransport transport;
    command_transport_init(&transport);

    assert(motor_command_mailbox_queue_control(&transport, control) == COMMAND_TRANSPORT_COMPLETE);
    assert_request(&transport, expected, sizeof(expected));
}

static void test_queues_command_write(void) {
    static const uint8_t command[] = {0x00, 0x00, 0x00, 0x90};
    static const uint8_t expected[] = {0x02, 0x40, 0x90, 0x00, 0x00, 0x00, 0x90};
    CommandTransport transport;
    command_transport_init(&transport);

    assert(motor_command_mailbox_queue_command(&transport, command) == COMMAND_TRANSPORT_COMPLETE);
    assert_request(&transport, expected, sizeof(expected));
}

static void test_reports_busy_transport(void) {
    static const uint8_t payload[] = {0x11};
    CommandTransport transport;
    command_transport_init(&transport);

    assert(motor_command_mailbox_queue_payload(&transport, payload, sizeof(payload)) ==
           COMMAND_TRANSPORT_COMPLETE);
    assert(motor_command_mailbox_queue_payload(&transport, payload, sizeof(payload)) ==
           COMMAND_TRANSPORT_BUSY);
}

static void test_completes_command_register_write(void) {
    static const uint8_t command[] = {0x12, 0x34, 0x56, 0x78};
    static const uint8_t accepted[] = {1};
    CommandTransport transport;
    MotorCommandMailboxWrite write;
    uint32_t command_value = 0;
    command_transport_init(&transport);
    motor_command_mailbox_write_init(&write, MOTOR_COMMAND_MAILBOX_COMMAND_WRITE);

    assert(motor_command_mailbox_write_run(&write, &transport, command, &command_value) ==
           MOTOR_COMMAND_MAILBOX_WRITE_NONE);
    assert(write.phase == MOTOR_COMMAND_MAILBOX_WRITE_WAIT);
    assert(command_transport_request_sent(&transport));
    assert(motor_command_mailbox_write_run(&write, &transport, command, &command_value) ==
           MOTOR_COMMAND_MAILBOX_WRITE_NONE);
    command_transport_receive(&transport, accepted, sizeof(accepted));
    assert(motor_command_mailbox_write_run(&write, &transport, command, &command_value) ==
           MOTOR_COMMAND_MAILBOX_WRITE_COMPLETE);
    assert(command_value == 0x12345678);
    assert(write.phase == MOTOR_COMMAND_MAILBOX_WRITE_QUEUE);
}

static void test_reports_rejected_register_write(void) {
    static const uint8_t control[] = {0x80, 0, 0, 0};
    static const uint8_t rejected[] = {0};
    CommandTransport transport;
    MotorCommandMailboxWrite write;
    command_transport_init(&transport);
    motor_command_mailbox_write_init(&write, MOTOR_COMMAND_MAILBOX_CONTROL_WRITE);

    assert(motor_command_mailbox_write_run(&write, &transport, control, 0) ==
           MOTOR_COMMAND_MAILBOX_WRITE_NONE);
    assert(command_transport_request_sent(&transport));
    command_transport_receive(&transport, rejected, sizeof(rejected));
    assert(motor_command_mailbox_write_run(&write, &transport, control, 0) ==
           MOTOR_COMMAND_MAILBOX_WRITE_NONE);
    assert(write.phase == MOTOR_COMMAND_MAILBOX_WRITE_REPORT_FAILURE);
    assert(motor_command_mailbox_write_run(&write, &transport, control, 0) ==
           MOTOR_COMMAND_MAILBOX_WRITE_FAILED);
    assert(write.phase == MOTOR_COMMAND_MAILBOX_WRITE_QUEUE);
}

int main(void) {
    test_queues_payload_write();
    test_queues_control_write();
    test_queues_command_write();
    test_reports_busy_transport();
    test_completes_command_register_write();
    test_reports_rejected_register_write();
    return 0;
}
