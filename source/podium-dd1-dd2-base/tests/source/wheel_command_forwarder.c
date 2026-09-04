#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "transfer/command.h"
#include "wheel/command_forwarder.h"

static void submit_request(CommandTransport *transport) {
    assert(command_transport_request_sent(transport));
}

static void complete_standard_probe(WheelCommandForwarder *forwarder, CommandTransport *transport) {
    wheel_command_forwarder_run(forwarder, transport);
    const uint8_t *request;
    uint16_t length;
    assert(command_transport_request(transport, &request, &length));
    const uint8_t expected[] = {2, 0x2b, 0x0c, 1, 0};
    assert(length == sizeof(expected));
    assert(memcmp(request, expected, sizeof(expected)) == 0);

    submit_request(transport);
    const uint8_t response[] = {1, 0, 0x55};
    command_transport_receive(transport, response, sizeof(response));
    wheel_command_forwarder_run(forwarder, transport);
    assert(forwarder->phase == WHEEL_COMMAND_FORWARDER_READY);
    assert(transport->owner == 0);
}

static void discovers_an_endpoint_when_work_arrives(void) {
    WheelCommandForwarder forwarder;
    CommandTransport transport;
    wheel_command_forwarder_init(&forwarder);
    command_transport_init(&transport);

    wheel_command_forwarder_run(&forwarder, &transport);
    assert(transport.phase == COMMAND_TRANSPORT_IDLE);

    const uint8_t payload[] = {3, 1, 2, 3, 0};
    assert(wheel_command_forwarder_queue(&forwarder, payload, sizeof(payload)));
    complete_standard_probe(&forwarder, &transport);
}

static void retains_endpoint_after_an_accepted_zero_probe(void) {
    WheelCommandForwarder forwarder;
    CommandTransport transport;
    wheel_command_forwarder_init(&forwarder);
    command_transport_init(&transport);
    const uint8_t payload[] = {1, 0xaa};
    assert(wheel_command_forwarder_queue(&forwarder, payload, sizeof(payload)));

    wheel_command_forwarder_run(&forwarder, &transport);
    submit_request(&transport);
    const uint8_t response[] = {1, 0, 0};
    command_transport_receive(&transport, response, sizeof(response));
    wheel_command_forwarder_run(&forwarder, &transport);
    assert(forwarder.endpoint_index == 0);
    assert(forwarder.phase == WHEEL_COMMAND_FORWARDER_READY);

    wheel_command_forwarder_run(&forwarder, &transport);
    const uint8_t *request;
    uint16_t length;
    assert(command_transport_request(&transport, &request, &length));
    const uint8_t expected[] = {2, 0x2a, 0xb0};
    assert(length == sizeof(expected) + sizeof(payload));
    assert(memcmp(request, expected, sizeof(expected)) == 0);
}

static void tries_the_extended_endpoint_after_a_failed_probe(void) {
    WheelCommandForwarder forwarder;
    CommandTransport transport;
    wheel_command_forwarder_init(&forwarder);
    command_transport_init(&transport);
    const uint8_t payload[] = {3, 1, 2, 3, 0};
    assert(wheel_command_forwarder_queue(&forwarder, payload, sizeof(payload)));

    wheel_command_forwarder_run(&forwarder, &transport);
    submit_request(&transport);
    command_transport_fail(&transport);
    wheel_command_forwarder_run(&forwarder, &transport);
    assert(forwarder.endpoint_index == 1);
    assert(forwarder.phase == WHEEL_COMMAND_FORWARDER_PROBE_READY);

    wheel_command_forwarder_run(&forwarder, &transport);
    const uint8_t *request;
    uint16_t length;
    assert(command_transport_request(&transport, &request, &length));
    const uint8_t expected[] = {2, 0x2d, 0x0c, 4, 0};
    assert(length == sizeof(expected));
    assert(memcmp(request, expected, sizeof(expected)) == 0);
}

static void forwards_batches_to_the_discovered_endpoint(void) {
    WheelCommandForwarder forwarder;
    CommandTransport transport;
    wheel_command_forwarder_init(&forwarder);
    command_transport_init(&transport);
    const uint8_t payload[] = {3, 0x81, 0x34, 0x12, 1, 0xaa};
    assert(wheel_command_forwarder_queue(&forwarder, payload, sizeof(payload)));
    complete_standard_probe(&forwarder, &transport);

    wheel_command_forwarder_run(&forwarder, &transport);
    const uint8_t *request;
    uint16_t length;
    assert(command_transport_request(&transport, &request, &length));
    const uint8_t header[] = {2, 0x2a, 0xb0};
    assert(length == sizeof(header) + sizeof(payload));
    assert(memcmp(request, header, sizeof(header)) == 0);
    assert(memcmp(request + sizeof(header), payload, sizeof(payload)) == 0);
    assert(wheel_command_forwarder_accepting(&forwarder));
    assert(forwarder.phase == WHEEL_COMMAND_FORWARDER_WRITE_PENDING);

    submit_request(&transport);
    const uint8_t response[] = {1};
    command_transport_receive(&transport, response, sizeof(response));
    wheel_command_forwarder_run(&forwarder, &transport);
    assert(forwarder.phase == WHEEL_COMMAND_FORWARDER_READY);
    assert(transport.owner == 0);
}

static void rejects_invalid_or_overlapping_batches(void) {
    WheelCommandForwarder forwarder;
    wheel_command_forwarder_init(&forwarder);
    uint8_t payload[WHEEL_COMMAND_FORWARDER_PAYLOAD_SIZE + 1] = {0};

    assert(!wheel_command_forwarder_queue(&forwarder, payload, 0));
    assert(!wheel_command_forwarder_queue(&forwarder, payload, sizeof(payload)));
    assert(!wheel_command_forwarder_queue(&forwarder, 0, 1));
    assert(wheel_command_forwarder_queue(&forwarder, payload, 1));
    assert(!wheel_command_forwarder_queue(&forwarder, payload, 1));
}

static void handles_transport_contention_and_failures(void) {
    WheelCommandForwarder forwarder;
    CommandTransport transport;
    const uint8_t payload[] = {1};
    wheel_command_forwarder_init(&forwarder);
    command_transport_init(&transport);
    assert(!wheel_command_forwarder_accepting(NULL));
    wheel_command_forwarder_run(NULL, &transport);
    wheel_command_forwarder_run(&forwarder, NULL);
    assert(wheel_command_forwarder_queue(&forwarder, payload, sizeof(payload)));

    command_transport_claim(&transport, 1);
    wheel_command_forwarder_run(&forwarder, &transport);
    assert(forwarder.phase == WHEEL_COMMAND_FORWARDER_PROBE_READY);
    command_transport_release(&transport, 1);

    transport.phase = COMMAND_TRANSPORT_READ_PENDING;
    transport.owner = 0x41;
    wheel_command_forwarder_run(&forwarder, &transport);
    assert(forwarder.phase == WHEEL_COMMAND_FORWARDER_PROBE_READY);
    transport.phase = COMMAND_TRANSPORT_IDLE;
    transport.completion = COMMAND_TRANSPORT_READ_REJECTED;
    wheel_command_forwarder_run(&forwarder, &transport);
    assert(forwarder.endpoint_index == 1);

    forwarder.phase = WHEEL_COMMAND_FORWARDER_PROBE_PENDING;
    transport.owner = 0x41;
    transport.phase = COMMAND_TRANSPORT_READ_PENDING;
    wheel_command_forwarder_run(&forwarder, &transport);
    assert(forwarder.phase == WHEEL_COMMAND_FORWARDER_PROBE_PENDING);
    transport.phase = COMMAND_TRANSPORT_IDLE;
    transport.completion = COMMAND_TRANSPORT_READ_REJECTED;
    wheel_command_forwarder_run(&forwarder, &transport);
    assert(forwarder.phase == WHEEL_COMMAND_FORWARDER_PROBE_READY);

    forwarder.phase = WHEEL_COMMAND_FORWARDER_READY;
    transport.owner = 1;
    wheel_command_forwarder_run(&forwarder, &transport);
    assert(forwarder.phase == WHEEL_COMMAND_FORWARDER_READY);
    transport.owner = 0x41;
    transport.phase = COMMAND_TRANSPORT_WRITE_PENDING;
    wheel_command_forwarder_run(&forwarder, &transport);
    assert(forwarder.phase == WHEEL_COMMAND_FORWARDER_READY);
    transport.phase = COMMAND_TRANSPORT_IDLE;
    transport.completion = COMMAND_TRANSPORT_WRITE_REJECTED;
    wheel_command_forwarder_run(&forwarder, &transport);
    assert(forwarder.phase == WHEEL_COMMAND_FORWARDER_PROBE_READY);

    forwarder.phase = WHEEL_COMMAND_FORWARDER_WRITE_PENDING;
    transport.owner = 0x41;
    transport.phase = COMMAND_TRANSPORT_WRITE_PENDING;
    wheel_command_forwarder_run(&forwarder, &transport);
    assert(forwarder.phase == WHEEL_COMMAND_FORWARDER_WRITE_PENDING);
    transport.phase = COMMAND_TRANSPORT_IDLE;
    transport.completion = COMMAND_TRANSPORT_WRITE_REJECTED;
    wheel_command_forwarder_run(&forwarder, &transport);
    assert(forwarder.phase == WHEEL_COMMAND_FORWARDER_PROBE_READY);
}

static void recovers_a_stalled_probe_after_the_wait_limit(void) {
    WheelCommandForwarder forwarder;
    CommandTransport transport;
    wheel_command_forwarder_init(&forwarder);
    command_transport_init(&transport);
    const uint8_t payload[] = {1};
    assert(wheel_command_forwarder_queue(&forwarder, payload, sizeof(payload)));

    wheel_command_forwarder_run(&forwarder, &transport);
    submit_request(&transport);
    for (uint16_t poll = 0; poll < 500; ++poll) {
        wheel_command_forwarder_run(&forwarder, &transport);
        assert(forwarder.phase == WHEEL_COMMAND_FORWARDER_PROBE_PENDING);
    }
    wheel_command_forwarder_run(&forwarder, &transport);
    assert(forwarder.endpoint_index == 1);
    assert(forwarder.phase == WHEEL_COMMAND_FORWARDER_PROBE_READY);
    assert(transport.phase == COMMAND_TRANSPORT_IDLE);
    assert(transport.owner == 0);
    assert(transport.completion == COMMAND_TRANSPORT_COMPLETE);

    wheel_command_forwarder_run(&forwarder, &transport);
    const uint8_t *request;
    uint16_t length;
    assert(command_transport_request(&transport, &request, &length));
    const uint8_t expected[] = {2, 0x2d, 0x0c, 4, 0};
    assert(length == sizeof(expected));
    assert(memcmp(request, expected, sizeof(expected)) == 0);
}

static void recovers_a_stalled_write_after_the_wait_limit(void) {
    WheelCommandForwarder forwarder;
    CommandTransport transport;
    wheel_command_forwarder_init(&forwarder);
    command_transport_init(&transport);
    const uint8_t payload[] = {1};
    assert(wheel_command_forwarder_queue(&forwarder, payload, sizeof(payload)));
    complete_standard_probe(&forwarder, &transport);

    wheel_command_forwarder_run(&forwarder, &transport);
    submit_request(&transport);
    for (uint16_t poll = 0; poll < 500; ++poll) {
        wheel_command_forwarder_run(&forwarder, &transport);
        assert(forwarder.phase == WHEEL_COMMAND_FORWARDER_WRITE_PENDING);
    }
    wheel_command_forwarder_run(&forwarder, &transport);
    assert(forwarder.endpoint_index == 1);
    assert(forwarder.phase == WHEEL_COMMAND_FORWARDER_PROBE_READY);
    assert(transport.phase == COMMAND_TRANSPORT_IDLE);
    assert(transport.owner == 0);
    assert(transport.completion == COMMAND_TRANSPORT_COMPLETE);

    assert(wheel_command_forwarder_queue(&forwarder, payload, sizeof(payload)));
    wheel_command_forwarder_run(&forwarder, &transport);
    const uint8_t *request;
    uint16_t length;
    assert(command_transport_request(&transport, &request, &length));
    const uint8_t expected[] = {2, 0x2d, 0x0c, 4, 0};
    assert(length == sizeof(expected));
    assert(memcmp(request, expected, sizeof(expected)) == 0);
}

int main(void) {
    discovers_an_endpoint_when_work_arrives();
    retains_endpoint_after_an_accepted_zero_probe();
    tries_the_extended_endpoint_after_a_failed_probe();
    forwards_batches_to_the_discovered_endpoint();
    rejects_invalid_or_overlapping_batches();
    handles_transport_contention_and_failures();
    recovers_a_stalled_probe_after_the_wait_limit();
    recovers_a_stalled_write_after_the_wait_limit();
    return 0;
}
