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

int main(void) {
    test_queues_payload_write();
    test_queues_control_write();
    test_queues_command_write();
    test_reports_busy_transport();
    return 0;
}
