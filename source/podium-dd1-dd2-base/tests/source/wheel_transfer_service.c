#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "platform/aux_bus.h"
#include "transfer/command.h"
#include "wheel/transfer_service.h"

static PlatformAuxBusStatus auxiliary_status;
static uint8_t auxiliary_address;
static uint16_t auxiliary_register;
static uint8_t auxiliary_write[WHEEL_TRANSFER_PAYLOAD_SIZE];
static uint8_t *auxiliary_read_destination;
static uint8_t auxiliary_write_count;
static uint8_t auxiliary_read_count;

void platform_aux_bus_init(void) {
    auxiliary_status = PLATFORM_AUX_BUS_IDLE;
    auxiliary_address = 0;
    auxiliary_register = 0;
    memset(auxiliary_write, 0, sizeof(auxiliary_write));
    auxiliary_read_destination = NULL;
    auxiliary_write_count = 0;
    auxiliary_read_count = 0;
}

void platform_aux_bus_service(void) {}

void platform_aux_bus_timer_tick(void) {}

bool platform_aux_bus_start_write(uint8_t address, uint16_t register_address, const uint8_t *data,
                                  uint16_t length) {
    if (auxiliary_status == PLATFORM_AUX_BUS_BUSY || data == NULL ||
        length != WHEEL_TRANSFER_PAYLOAD_SIZE) {
        return false;
    }
    auxiliary_status = PLATFORM_AUX_BUS_BUSY;
    auxiliary_address = address;
    auxiliary_register = register_address;
    memcpy(auxiliary_write, data, length);
    auxiliary_write_count++;
    return true;
}

bool platform_aux_bus_start_read(uint8_t address, uint16_t register_address, uint8_t *data,
                                 uint16_t length) {
    if (auxiliary_status == PLATFORM_AUX_BUS_BUSY || data == NULL ||
        length != WHEEL_TRANSFER_PAYLOAD_SIZE) {
        return false;
    }
    auxiliary_status = PLATFORM_AUX_BUS_BUSY;
    auxiliary_address = address;
    auxiliary_register = register_address;
    auxiliary_read_destination = data;
    auxiliary_read_count++;
    return true;
}

PlatformAuxBusStatus platform_aux_bus_status(void) { return auxiliary_status; }

void platform_aux_bus_clear(void) {
    if (auxiliary_status != PLATFORM_AUX_BUS_BUSY) {
        auxiliary_status = PLATFORM_AUX_BUS_IDLE;
    }
}

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
    assert(service->phase == WHEEL_TRANSFER_PHASE_READ_READY);
    assert(!command_transport_request(transport, &message, &length));
    wheel_transfer_service_run(service, transport);
    assert(command_transport_request(transport, &message, &length));
    assert(length == 5);
    assert(message[0] == 2);
    assert(message[1] == (uint8_t)(((request == WHEEL_TRANSFER_READ ? 0x30 : 0x31) << 1) | 1));
    assert(message[2] == 0);
    assert(message[3] == WHEEL_TRANSFER_PAYLOAD_SIZE);
    assert(message[4] == 0);
}

static void test_defers_command_read_until_following_pass(void) {
    WheelTransferService service;
    CommandTransport transport;
    const uint8_t *message;
    uint16_t length;
    static const uint8_t accepted[] = {1};

    wheel_transfer_service_init(&service);
    command_transport_init(&transport);
    assert(wheel_transfer_service_start(&service, WHEEL_TRANSFER_READ));
    wheel_transfer_service_run(&service, &transport);
    assert(command_transport_request(&transport, &message, &length));
    submit_and_respond(&transport, accepted, sizeof(accepted));

    wheel_transfer_service_run(&service, &transport);
    assert(service.phase == WHEEL_TRANSFER_PHASE_READ_READY);
    assert(transport.owner == 0x30);
    assert(transport.phase == COMMAND_TRANSPORT_IDLE);
    assert(!command_transport_request(&transport, &message, &length));

    wheel_transfer_service_run(&service, &transport);
    assert(service.phase == WHEEL_TRANSFER_PHASE_READ_PENDING);
    assert(command_transport_request(&transport, &message, &length));
}

static void test_completes_valid_command_read_channel(void) {
    WheelTransferService service;
    CommandTransport transport;
    wheel_transfer_service_init(&service);
    command_transport_init(&transport);
    begin_read(&service, &transport, WHEEL_TRANSFER_READ);

    static const uint8_t response[] = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xaa};
    submit_and_respond(&transport, response, sizeof(response));
    wheel_transfer_service_run(&service, &transport);

    assert(wheel_transfer_service_status(&service, WHEEL_TRANSFER_READ) == WHEEL_TRANSFER_COMPLETE);
    assert(service.phase == WHEEL_TRANSFER_PHASE_IDLE);
    assert(transport.owner == 0);
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

static void test_maps_command_direction_failures(void) {
    WheelTransferService service;
    CommandTransport transport;
    wheel_transfer_service_init(&service);
    command_transport_init(&transport);
    assert(wheel_transfer_service_start(&service, WHEEL_TRANSFER_READ));
    wheel_transfer_service_run(&service, &transport);

    static const uint8_t rejected[] = {0};
    submit_and_respond(&transport, rejected, sizeof(rejected));
    wheel_transfer_service_run(&service, &transport);
    assert(wheel_transfer_service_status(&service, WHEEL_TRANSFER_READ) ==
           WHEEL_TRANSFER_WRITE_FAILED);

    assert(wheel_transfer_service_start(&service, WHEEL_TRANSFER_READ));
    wheel_transfer_service_run(&service, &transport);
    static const uint8_t accepted[] = {1};
    submit_and_respond(&transport, accepted, sizeof(accepted));
    wheel_transfer_service_run(&service, &transport);
    wheel_transfer_service_run(&service, &transport);
    submit_and_respond(&transport, rejected, sizeof(rejected));
    wheel_transfer_service_run(&service, &transport);
    assert(wheel_transfer_service_status(&service, WHEEL_TRANSFER_READ) ==
           WHEEL_TRANSFER_READ_FAILED);
}

static void test_routes_write_channel_over_auxiliary_bus(void) {
    WheelTransferService service;
    CommandTransport transport;
    static const uint8_t response[WHEEL_TRANSFER_PAYLOAD_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0xaa};
    wheel_transfer_service_init(&service);
    command_transport_init(&transport);
    platform_aux_bus_init();

    assert(wheel_transfer_service_start(&service, WHEEL_TRANSFER_WRITE));
    wheel_transfer_service_run(&service, &transport);
    assert(service.phase == WHEEL_TRANSFER_PHASE_WRITE_PENDING);
    assert(auxiliary_write_count == 1);
    assert(auxiliary_address == 0x31 && auxiliary_register == 0);
    assert(memcmp(auxiliary_write, "EndOfLine+", WHEEL_TRANSFER_PAYLOAD_SIZE) == 0);
    assert(transport.owner == 0);

    wheel_transfer_service_run(&service, &transport);
    assert(service.phase == WHEEL_TRANSFER_PHASE_WRITE_PENDING);
    assert(auxiliary_write_count == 1);

    auxiliary_status = PLATFORM_AUX_BUS_SUCCEEDED;
    wheel_transfer_service_run(&service, &transport);
    assert(service.phase == WHEEL_TRANSFER_PHASE_READ_READY);
    assert(platform_aux_bus_status() == PLATFORM_AUX_BUS_IDLE);

    wheel_transfer_service_run(&service, &transport);
    assert(service.phase == WHEEL_TRANSFER_PHASE_READ_PENDING);
    assert(auxiliary_read_count == 1);
    assert(auxiliary_address == 0x31 && auxiliary_register == 0);
    memcpy(auxiliary_read_destination, response, sizeof(response));

    wheel_transfer_service_run(&service, &transport);
    assert(service.phase == WHEEL_TRANSFER_PHASE_READ_PENDING);
    auxiliary_status = PLATFORM_AUX_BUS_SUCCEEDED;
    wheel_transfer_service_run(&service, &transport);
    assert(service.phase == WHEEL_TRANSFER_PHASE_IDLE);
    assert(wheel_transfer_service_status(&service, WHEEL_TRANSFER_WRITE) ==
           WHEEL_TRANSFER_COMPLETE);
    assert(platform_aux_bus_status() == PLATFORM_AUX_BUS_IDLE);
}

static void test_preserves_another_auxiliary_owner_result(void) {
    WheelTransferService service;
    CommandTransport transport;
    wheel_transfer_service_init(&service);
    command_transport_init(&transport);
    platform_aux_bus_init();
    auxiliary_status = PLATFORM_AUX_BUS_SUCCEEDED;

    assert(wheel_transfer_service_start(&service, WHEEL_TRANSFER_WRITE));
    wheel_transfer_service_run(&service, &transport);
    assert(service.phase == WHEEL_TRANSFER_PHASE_WRITE_READY);
    assert(auxiliary_write_count == 0);
    assert(platform_aux_bus_status() == PLATFORM_AUX_BUS_SUCCEEDED);

    platform_aux_bus_clear();
    wheel_transfer_service_run(&service, &transport);
    assert(service.phase == WHEEL_TRANSFER_PHASE_WRITE_PENDING);
    assert(auxiliary_write_count == 1);
}

static void test_reports_auxiliary_write_and_read_failures(void) {
    WheelTransferService service;
    CommandTransport transport;
    wheel_transfer_service_init(&service);
    command_transport_init(&transport);
    platform_aux_bus_init();

    assert(wheel_transfer_service_start(&service, WHEEL_TRANSFER_WRITE));
    wheel_transfer_service_run(&service, &transport);
    auxiliary_status = PLATFORM_AUX_BUS_FAILED;
    wheel_transfer_service_run(&service, &transport);
    assert(wheel_transfer_service_status(&service, WHEEL_TRANSFER_WRITE) ==
           WHEEL_TRANSFER_WRITE_FAILED);
    assert(service.phase == WHEEL_TRANSFER_PHASE_IDLE);

    assert(wheel_transfer_service_start(&service, WHEEL_TRANSFER_WRITE));
    wheel_transfer_service_run(&service, &transport);
    auxiliary_status = PLATFORM_AUX_BUS_SUCCEEDED;
    wheel_transfer_service_run(&service, &transport);
    wheel_transfer_service_run(&service, &transport);
    auxiliary_status = PLATFORM_AUX_BUS_FAILED;
    wheel_transfer_service_run(&service, &transport);
    assert(wheel_transfer_service_status(&service, WHEEL_TRANSFER_WRITE) ==
           WHEEL_TRANSFER_READ_FAILED);
    assert(service.phase == WHEEL_TRANSFER_PHASE_IDLE);
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
    assert(wheel_transfer_service_start(&service, WHEEL_TRANSFER_READ));
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

static void test_queues_native_payloads_separately_from_handshake(void) {
    WheelTransferService service;
    wheel_transfer_service_init(&service);
    uint8_t payload[WHEEL_TRANSFER_NATIVE_PAYLOAD_CAPACITY];
    for (uint8_t index = 0; index < sizeof(payload); index++) {
        payload[index] = index;
    }

    assert(wheel_transfer_service_queue_native_payload(&service, payload, sizeof(payload)));
    const WheelTransferNativePayload *queued = wheel_transfer_service_native_payload(&service);
    assert(queued != NULL && queued->length == sizeof(payload));
    assert(memcmp(queued->data, payload, sizeof(payload)) == 0);
    wheel_transfer_service_release_native_payload(&service);
    assert(wheel_transfer_service_native_payload(&service) == NULL);
    assert(!wheel_transfer_service_queue_native_payload(&service, NULL, 1));
    assert(!wheel_transfer_service_queue_native_payload(&service, payload, 0));
    assert(!wheel_transfer_service_queue_native_payload(
        &service, payload, WHEEL_TRANSFER_NATIVE_PAYLOAD_CAPACITY + 1));
}

int main(void) {
    test_completes_valid_command_read_channel();
    test_rejects_invalid_response_crc();
    test_maps_command_direction_failures();
    test_defers_command_read_until_following_pass();
    test_routes_write_channel_over_auxiliary_bus();
    test_preserves_another_auxiliary_owner_result();
    test_reports_auxiliary_write_and_read_failures();
    test_waits_for_another_command_owner();
    test_rejects_invalid_and_overlapping_requests();
    test_queues_native_payloads_separately_from_handshake();
    return 0;
}
