#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "transfer/command.h"

static void submit(CommandTransport *transport, const uint8_t *expected, uint16_t expected_length) {
    const uint8_t *request;
    uint16_t length;
    assert(command_transport_request(transport, &request, &length));
    assert(length == expected_length);
    assert(memcmp(request, expected, length) == 0);
    assert(command_transport_request_sent(transport));
    assert(!command_transport_request(transport, &request, &length));
}

static void test_tracks_ownership(void) {
    CommandTransport transport;
    command_transport_init(&transport);
    assert(command_transport_is_owner(&transport, 0));

    command_transport_claim(&transport, 0x20);
    command_transport_claim(&transport, 0x30);
    assert(command_transport_is_owner(&transport, 0x20));
    assert(command_transport_poll(&transport, 0x30) == COMMAND_TRANSPORT_BUSY);

    command_transport_release(&transport, 0x30);
    assert(command_transport_is_owner(&transport, 0x20));
    command_transport_release(&transport, 0x20);
    assert(command_transport_is_owner(&transport, 0));
}

static void test_completes_write(void) {
    CommandTransport transport;
    command_transport_init(&transport);
    const uint8_t data[] = {0xaa, 0xbb};
    const uint8_t expected[] = {2, 0x40, 0x90, 0xaa, 0xbb};

    assert(command_transport_queue_write(&transport, 0x20, 0x90, data, sizeof(data)) ==
           COMMAND_TRANSPORT_COMPLETE);
    assert(command_transport_poll(&transport, 0x20) == COMMAND_TRANSPORT_BUSY);
    submit(&transport, expected, sizeof(expected));
    const uint8_t accepted[] = {1};
    command_transport_receive(&transport, accepted, sizeof(accepted));
    assert(command_transport_poll(&transport, 0x20) == COMMAND_TRANSPORT_COMPLETE);
}

static void test_completes_read(void) {
    CommandTransport transport;
    command_transport_init(&transport);
    uint8_t output[3] = {0};
    const uint8_t expected[] = {2, 0x41, 0x80, 3, 0};

    assert(command_transport_queue_read(&transport, 0x20, 0x80, output, sizeof(output)) ==
           COMMAND_TRANSPORT_COMPLETE);
    submit(&transport, expected, sizeof(expected));
    const uint8_t response[] = {1, 0, 0x11, 0x22, 0x33};
    command_transport_receive(&transport, response, sizeof(response));
    assert(memcmp(output, response + 2, sizeof(output)) == 0);
    assert(command_transport_poll(&transport, 0x20) == COMMAND_TRANSPORT_COMPLETE);
}

static void test_latches_rejections(void) {
    CommandTransport transport;
    command_transport_init(&transport);
    const uint8_t rejected[] = {0};
    const uint8_t write[] = {2, 0x40, 0};

    assert(command_transport_queue_write(&transport, 0x20, 0, 0, 0) == COMMAND_TRANSPORT_COMPLETE);
    submit(&transport, write, sizeof(write));
    command_transport_receive(&transport, rejected, sizeof(rejected));
    assert(command_transport_queue_write(&transport, 0x20, 0, 0, 0) == COMMAND_TRANSPORT_BUSY);
    assert(command_transport_poll(&transport, 0x20) == COMMAND_TRANSPORT_WRITE_REJECTED);
    assert(command_transport_poll(&transport, 0x20) == COMMAND_TRANSPORT_COMPLETE);

    const uint8_t read[] = {2, 0x41, 0, 0, 0};
    assert(command_transport_queue_read(&transport, 0x20, 0, 0, 0) == COMMAND_TRANSPORT_COMPLETE);
    submit(&transport, read, sizeof(read));
    command_transport_receive(&transport, rejected, sizeof(rejected));
    assert(command_transport_poll(&transport, 0x20) == COMMAND_TRANSPORT_READ_REJECTED);
}

static void test_rejects_invalid_requests_and_responses(void) {
    CommandTransport transport;
    command_transport_init(&transport);
    uint8_t output[1];

    assert(
        command_transport_queue_read(&transport, 1, 0, output, MEMORY_TRANSFER_MAX_READ_SIZE + 1) ==
        COMMAND_TRANSPORT_TOO_LONG);
    assert(command_transport_queue_read(&transport, 1, 0, 0, 1) == COMMAND_TRANSPORT_TOO_LONG);
    assert(command_transport_queue_write(&transport, 1, 0, 0, 1) == COMMAND_TRANSPORT_TOO_LONG);

    assert(command_transport_queue_read(&transport, 1, 0, output, sizeof(output)) ==
           COMMAND_TRANSPORT_COMPLETE);
    assert(command_transport_request_sent(&transport));
    const uint8_t malformed[] = {1, 0};
    command_transport_receive(&transport, malformed, sizeof(malformed));
    assert(command_transport_poll(&transport, 1) == COMMAND_TRANSPORT_BUSY);

    command_transport_init(&transport);
    command_transport_receive(&transport, malformed, sizeof(malformed));
    assert(command_transport_poll(&transport, 0) == COMMAND_TRANSPORT_WRITE_REJECTED);
}

int main(void) {
    test_tracks_ownership();
    test_completes_write();
    test_completes_read();
    test_latches_rejections();
    test_rejects_invalid_requests_and_responses();
    return 0;
}
