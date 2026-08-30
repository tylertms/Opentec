#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "transfer/command.h"
#include "wheel/transfer_service.h"

static void submit_and_respond(CommandTransport *transport, const uint8_t *response,
                               uint16_t length) {
    assert(command_transport_request_sent(transport));
    command_transport_receive(transport, response, length);
}

static void begin_read(WheelTransferService *service, CommandTransport *transport,
                       WheelTransferRequest request) {
    assert(wheel_transfer_service_start(service, request));
    wheel_transfer_service_run(service, transport);
    const uint8_t *message;
    uint16_t length;
    assert(command_transport_request(transport, &message, &length));
    assert(length == 13);
    assert(message[0] == 2);
    assert(message[1] == (uint8_t)((request == WHEEL_TRANSFER_READ ? 0x30 : 0x31) << 1));
    assert(message[2] == 0);
    assert(memcmp(message + 3, "EndOfLine+", WHEEL_TRANSFER_PAYLOAD_SIZE) == 0);

    static const uint8_t accepted[] = {1};
    submit_and_respond(transport, accepted, sizeof(accepted));
    wheel_transfer_service_run(service, transport);
    assert(command_transport_request(transport, &message, &length));
    assert(length == 5);
    assert(message[0] == 2);
    assert(message[1] == (uint8_t)(((request == WHEEL_TRANSFER_READ ? 0x30 : 0x31) << 1) | 1));
    assert(message[2] == 0);
    assert(message[3] == WHEEL_TRANSFER_PAYLOAD_SIZE);
    assert(message[4] == 0);
}

static void test_completes_valid_write_and_read_channels(void) {
    for (WheelTransferRequest request = WHEEL_TRANSFER_WRITE;
         request < WHEEL_TRANSFER_REQUEST_COUNT; request++) {
        WheelTransferService service;
        CommandTransport transport;
        wheel_transfer_service_init(&service);
        command_transport_init(&transport);
        begin_read(&service, &transport, request);

        static const uint8_t response[] = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xaa};
        submit_and_respond(&transport, response, sizeof(response));
        wheel_transfer_service_run(&service, &transport);

        assert(wheel_transfer_service_status(&service, request) == WHEEL_TRANSFER_COMPLETE);
        assert(service.phase == WHEEL_TRANSFER_PHASE_IDLE);
        assert(transport.owner == 0);
    }
}

static void test_rejects_invalid_response_crc(void) {
    WheelTransferService service;
    CommandTransport transport;
    wheel_transfer_service_init(&service);
    command_transport_init(&transport);
    begin_read(&service, &transport, WHEEL_TRANSFER_READ);

    static const uint8_t response[12] = {1};
    submit_and_respond(&transport, response, sizeof(response));
    wheel_transfer_service_run(&service, &transport);

    assert(wheel_transfer_service_status(&service, WHEEL_TRANSFER_READ) ==
           WHEEL_TRANSFER_INVALID_RESPONSE);
}

static void test_maps_direction_failures(void) {
    WheelTransferService service;
    CommandTransport transport;
    wheel_transfer_service_init(&service);
    command_transport_init(&transport);
    assert(wheel_transfer_service_start(&service, WHEEL_TRANSFER_WRITE));
    wheel_transfer_service_run(&service, &transport);

    static const uint8_t rejected[] = {0};
    submit_and_respond(&transport, rejected, sizeof(rejected));
    wheel_transfer_service_run(&service, &transport);
    assert(wheel_transfer_service_status(&service, WHEEL_TRANSFER_WRITE) ==
           WHEEL_TRANSFER_WRITE_FAILED);

    assert(wheel_transfer_service_start(&service, WHEEL_TRANSFER_READ));
    wheel_transfer_service_run(&service, &transport);
    static const uint8_t accepted[] = {1};
    submit_and_respond(&transport, accepted, sizeof(accepted));
    wheel_transfer_service_run(&service, &transport);
    submit_and_respond(&transport, rejected, sizeof(rejected));
    wheel_transfer_service_run(&service, &transport);
    assert(wheel_transfer_service_status(&service, WHEEL_TRANSFER_READ) ==
           WHEEL_TRANSFER_READ_FAILED);
}

static void test_waits_for_another_command_owner(void) {
    WheelTransferService service;
    CommandTransport transport;
    wheel_transfer_service_init(&service);
    command_transport_init(&transport);
    command_transport_claim(&transport, 0x20);
    assert(wheel_transfer_service_start(&service, WHEEL_TRANSFER_READ));

    wheel_transfer_service_run(&service, &transport);
    assert(service.phase == WHEEL_TRANSFER_PHASE_WRITE_READY);
    assert(wheel_transfer_service_status(&service, WHEEL_TRANSFER_READ) == WHEEL_TRANSFER_PENDING);

    command_transport_release(&transport, 0x20);
    wheel_transfer_service_run(&service, &transport);
    assert(service.phase == WHEEL_TRANSFER_PHASE_WRITE_PENDING);
    assert(transport.owner == 0x30);
}

static void test_rejects_invalid_and_overlapping_requests(void) {
    WheelTransferService service;
    CommandTransport transport;
    wheel_transfer_service_init(&service);
    command_transport_init(&transport);

    assert(!wheel_transfer_service_start(NULL, WHEEL_TRANSFER_READ));
    assert(!wheel_transfer_service_start(&service, WHEEL_TRANSFER_REQUEST_COUNT));
    assert(wheel_transfer_service_start(&service, WHEEL_TRANSFER_WRITE));
    assert(!wheel_transfer_service_start(&service, WHEEL_TRANSFER_READ));
    wheel_transfer_service_run(NULL, &transport);
    wheel_transfer_service_run(&service, NULL);
    WheelTransferService idle;
    wheel_transfer_service_init(&idle);
    wheel_transfer_service_run(&idle, &transport);
    assert(wheel_transfer_service_status(NULL, WHEEL_TRANSFER_READ) == WHEEL_TRANSFER_IDLE);
    assert(wheel_transfer_service_status(&service, WHEEL_TRANSFER_REQUEST_COUNT) ==
           WHEEL_TRANSFER_IDLE);
}

int main(void) {
    test_completes_valid_write_and_read_channels();
    test_rejects_invalid_response_crc();
    test_maps_direction_failures();
    test_waits_for_another_command_owner();
    test_rejects_invalid_and_overlapping_requests();
    return 0;
}
