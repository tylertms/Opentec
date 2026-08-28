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

int main(void) {
    discovers_an_endpoint_when_work_arrives();
    tries_the_extended_endpoint_after_a_failed_probe();
    forwards_batches_to_the_discovered_endpoint();
    rejects_invalid_or_overlapping_batches();
    return 0;
}
