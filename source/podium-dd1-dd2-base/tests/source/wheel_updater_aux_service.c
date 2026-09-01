#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform/aux_bus.h"
#include "wheel/updater_aux_service.h"

static PlatformAuxBusStatus bus_status;
static uint8_t requested_address;
static uint16_t requested_register;
static uint8_t requested_data[WHEEL_UPDATER_BRIDGE_MAX_REQUEST_SIZE];
static uint8_t *read_destination;
static uint16_t requested_length;
static uint8_t start_count;

bool platform_aux_bus_start_write(uint8_t address, uint16_t register_address, const uint8_t *data,
                                  uint16_t length) {
    if (bus_status != PLATFORM_AUX_BUS_IDLE) {
        return false;
    }
    requested_address = address;
    requested_register = register_address;
    requested_length = length;
    memcpy(requested_data, data, length);
    read_destination = NULL;
    start_count++;
    bus_status = PLATFORM_AUX_BUS_BUSY;
    return true;
}

bool platform_aux_bus_start_read(uint8_t address, uint16_t register_address, uint8_t *data,
                                 uint16_t length) {
    if (bus_status != PLATFORM_AUX_BUS_IDLE) {
        return false;
    }
    requested_address = address;
    requested_register = register_address;
    requested_length = length;
    read_destination = data;
    start_count++;
    bus_status = PLATFORM_AUX_BUS_BUSY;
    return true;
}

PlatformAuxBusStatus platform_aux_bus_status(void) { return bus_status; }

void platform_aux_bus_clear(void) { bus_status = PLATFORM_AUX_BUS_IDLE; }

static void reset_bus(void) {
    bus_status = PLATFORM_AUX_BUS_IDLE;
    requested_address = 0;
    requested_register = 0;
    memset(requested_data, 0, sizeof(requested_data));
    read_destination = NULL;
    requested_length = 0;
    start_count = 0;
}

static void finish_read(const uint8_t *data) {
    memcpy(read_destination, data, requested_length);
    bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
}

static void complete_handshake(WheelUpdaterAuxService *service) {
    wheel_updater_aux_service_request_handshake(service);
    wheel_updater_aux_service_run(service, 0);
    bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
    wheel_updater_aux_service_run(service, 0);
    assert(wheel_updater_aux_service_handshake_complete(service));
}

static void test_retries_handshake_until_success(void) {
    WheelUpdaterAuxService service;
    reset_bus();
    wheel_updater_aux_service_init(&service);
    wheel_updater_aux_service_request_handshake(&service);
    wheel_updater_aux_service_run(&service, 0);

    const uint8_t expected[] = {0xfa, 0x05};
    assert(requested_address == 0x78);
    assert(requested_register == 3);
    assert(requested_length == sizeof(expected));
    assert(memcmp(requested_data, expected, sizeof(expected)) == 0);
    assert(wheel_updater_aux_service_active(&service));

    bus_status = PLATFORM_AUX_BUS_FAILED;
    wheel_updater_aux_service_run(&service, 1);
    assert(!wheel_updater_aux_service_handshake_complete(&service));
    wheel_updater_aux_service_run(&service, 2);
    assert(start_count == 2);

    bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
    wheel_updater_aux_service_run(&service, 3);
    assert(wheel_updater_aux_service_handshake_complete(&service));
    assert(!wheel_updater_aux_service_active(&service));
}

static void test_exchanges_acknowledgement_response(void) {
    WheelUpdaterAuxService service;
    const uint8_t request[] = {0x5a, 0xb0, 0x33};
    reset_bus();
    wheel_updater_aux_service_init(&service);
    assert(!wheel_updater_aux_service_start(&service, request, sizeof(request)));
    complete_handshake(&service);
    assert(wheel_updater_aux_service_start(&service, request, sizeof(request)));

    wheel_updater_aux_service_run(&service, 100);
    assert(requested_address == 0x10);
    assert(requested_register == 0);
    assert(requested_length == sizeof(request));
    assert(memcmp(requested_data, request, sizeof(request)) == 0);

    bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
    wheel_updater_aux_service_run(&service, 100);
    wheel_updater_aux_service_run(&service, 101);
    assert(bus_status == PLATFORM_AUX_BUS_IDLE);
    wheel_updater_aux_service_run(&service, 102);
    assert(requested_address == 0x10);
    assert(requested_register == 0);
    assert(requested_length == 1);

    const uint8_t marker[] = {0x5a};
    finish_read(marker);
    wheel_updater_aux_service_run(&service, 102);
    assert(requested_length == 1);
    const uint8_t opcode[] = {0xa2};
    finish_read(opcode);
    wheel_updater_aux_service_run(&service, 102);

    const uint8_t *response = NULL;
    uint8_t response_length = 0;
    assert(wheel_updater_aux_service_take_response(&service, &response, &response_length));
    const uint8_t expected[] = {0x5a, 0xa2};
    assert(response_length == sizeof(expected));
    assert(memcmp(response, expected, sizeof(expected)) == 0);
    assert(!wheel_updater_aux_service_active(&service));
}

static void test_aborts_failed_updater_operation(void) {
    WheelUpdaterAuxService service;
    const uint8_t request[] = {0x5a, 0xa1};
    reset_bus();
    wheel_updater_aux_service_init(&service);
    complete_handshake(&service);
    assert(wheel_updater_aux_service_start(&service, request, sizeof(request)));

    wheel_updater_aux_service_run(&service, 0);
    uint8_t starts_before_failure = start_count;
    bus_status = PLATFORM_AUX_BUS_FAILED;
    wheel_updater_aux_service_run(&service, 1);
    assert(start_count == starts_before_failure);
    assert(!wheel_updater_aux_service_active(&service));
}

static void test_prepares_startup_recovery_without_handshake(void) {
    WheelUpdaterAuxService service;
    const uint8_t request[] = {0x5a, 0xa7};
    reset_bus();
    wheel_updater_aux_service_init(&service);
    wheel_updater_aux_service_prepare_startup_recovery(&service);
    assert(wheel_updater_aux_service_handshake_complete(&service));
    assert(wheel_updater_aux_service_start(&service, request, sizeof(request)));

    wheel_updater_aux_service_run(&service, 0);
    assert(requested_address == 0x10);
    assert(requested_register == 0);
    assert(requested_length == sizeof(request));
    assert(memcmp(requested_data, request, sizeof(request)) == 0);
}

int main(void) {
    test_retries_handshake_until_success();
    test_exchanges_acknowledgement_response();
    test_aborts_failed_updater_operation();
    test_prepares_startup_recovery_without_handshake();
    assert(!wheel_updater_aux_service_handshake_complete(NULL));
    assert(!wheel_updater_aux_service_active(NULL));
    return 0;
}
