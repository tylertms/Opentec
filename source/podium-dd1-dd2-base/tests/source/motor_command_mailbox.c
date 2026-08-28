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

static void complete_read(CommandTransport *transport, const uint8_t *data, uint16_t length) {
    uint8_t response[MEMORY_TRANSFER_MAX_READ_SIZE + 2] = {1, 0};
    memcpy(response + 2, data, length);
    assert(command_transport_request_sent(transport));
    command_transport_receive(transport, response, length + 2);
}

static void complete_write(CommandTransport *transport) {
    static const uint8_t accepted[] = {1};
    assert(command_transport_request_sent(transport));
    command_transport_receive(transport, accepted, sizeof(accepted));
}

static void test_exchanges_outgoing_packet(void) {
    CommandTransport transport;
    MotorCommandMailboxExchange exchange;
    uint8_t read_buffer[16];
    static const uint8_t packet[] = {0x11, 0x22, 0x33};
    static const uint8_t control[] = {0, 0, 0, 0};
    command_transport_init(&transport);
    assert(motor_command_mailbox_exchange_init(&exchange, read_buffer, sizeof(read_buffer)));
    assert(motor_command_mailbox_exchange_queue(&exchange, packet, sizeof(packet)));
    assert(motor_command_mailbox_exchange_queue(&exchange, packet, 2));
    assert(exchange.write_length == 2);
    exchange.write_length = sizeof(packet);

    assert(motor_command_mailbox_exchange_run(&exchange, &transport).event ==
           MOTOR_COMMAND_MAILBOX_EXCHANGE_NONE);
    complete_read(&transport, control, sizeof(control));
    assert(motor_command_mailbox_exchange_run(&exchange, &transport).event ==
           MOTOR_COMMAND_MAILBOX_EXCHANGE_NONE);
    static const uint8_t expected[] = {2, 0x40, 0x80, 0x11, 0x22, 0x33};
    assert_request(&transport, expected, sizeof(expected));

    complete_write(&transport);
    assert(motor_command_mailbox_exchange_run(&exchange, &transport).event ==
           MOTOR_COMMAND_MAILBOX_EXCHANGE_PACKET_WRITTEN);
    assert(exchange.write_packet == 0);
}

static void test_exchanges_incoming_packet_first(void) {
    CommandTransport transport;
    MotorCommandMailboxExchange exchange;
    uint8_t read_buffer[16];
    static const uint8_t outgoing[] = {0xaa};
    static const uint8_t control[] = {0x40, 0, 0, 3};
    static const uint8_t incoming[] = {0x44, 0x55, 0x66};
    command_transport_init(&transport);
    assert(motor_command_mailbox_exchange_init(&exchange, read_buffer, sizeof(read_buffer)));
    assert(motor_command_mailbox_exchange_queue(&exchange, outgoing, sizeof(outgoing)));

    (void)motor_command_mailbox_exchange_run(&exchange, &transport);
    complete_read(&transport, control, sizeof(control));
    assert(motor_command_mailbox_exchange_run(&exchange, &transport).event ==
           MOTOR_COMMAND_MAILBOX_EXCHANGE_NONE);
    static const uint8_t expected[] = {2, 0x41, 0x80, 3, 0};
    assert_request(&transport, expected, sizeof(expected));

    complete_read(&transport, incoming, sizeof(incoming));
    MotorCommandMailboxExchangeResult result =
        motor_command_mailbox_exchange_run(&exchange, &transport);
    assert(result.event == MOTOR_COMMAND_MAILBOX_EXCHANGE_PACKET_READ);
    assert(result.packet_length == sizeof(incoming));
    assert(memcmp(result.packet, incoming, sizeof(incoming)) == 0);
    assert(exchange.write_packet == outgoing);
}

static void test_reads_status_after_second_retry(void) {
    CommandTransport transport;
    MotorCommandMailboxExchange exchange;
    uint8_t read_buffer[16];
    static const uint8_t control[] = {0x80, 0, 0, 0};
    static const uint8_t status[] = {0x12, 0x34, 0x56, 0x78};
    command_transport_init(&transport);
    assert(motor_command_mailbox_exchange_init(&exchange, read_buffer, sizeof(read_buffer)));

    (void)motor_command_mailbox_exchange_run(&exchange, &transport);
    complete_read(&transport, control, sizeof(control));
    assert(motor_command_mailbox_exchange_run(&exchange, &transport).event ==
           MOTOR_COMMAND_MAILBOX_EXCHANGE_NONE);
    assert(exchange.status_retry_count == 1);

    (void)motor_command_mailbox_exchange_run(&exchange, &transport);
    complete_read(&transport, control, sizeof(control));
    assert(motor_command_mailbox_exchange_run(&exchange, &transport).event ==
           MOTOR_COMMAND_MAILBOX_EXCHANGE_NONE);
    static const uint8_t expected[] = {2, 0x41, 0x90, 4, 0};
    assert_request(&transport, expected, sizeof(expected));

    complete_read(&transport, status, sizeof(status));
    MotorCommandMailboxExchangeResult result =
        motor_command_mailbox_exchange_run(&exchange, &transport);
    assert(result.event == MOTOR_COMMAND_MAILBOX_EXCHANGE_STATUS_READ);
    assert(result.status == 0x12345678);
    assert(exchange.status_retry_count == 0);
}

static void test_rejects_oversized_incoming_packet(void) {
    CommandTransport transport;
    MotorCommandMailboxExchange exchange;
    uint8_t read_buffer[3];
    static const uint8_t control[] = {0x40, 0, 0, 4};
    command_transport_init(&transport);
    assert(motor_command_mailbox_exchange_init(&exchange, read_buffer, sizeof(read_buffer)));

    (void)motor_command_mailbox_exchange_run(&exchange, &transport);
    complete_read(&transport, control, sizeof(control));
    assert(motor_command_mailbox_exchange_run(&exchange, &transport).event ==
           MOTOR_COMMAND_MAILBOX_EXCHANGE_FAILED);
}

int main(void) {
    test_queues_payload_read();
    test_queues_payload_write();
    test_queues_length_read();
    test_queues_control_read();
    test_queues_status_read();
    test_waits_for_active_transport();
    test_decodes_control();
    test_exchanges_outgoing_packet();
    test_exchanges_incoming_packet_first();
    test_reads_status_after_second_retry();
    test_rejects_oversized_incoming_packet();
    return 0;
}
