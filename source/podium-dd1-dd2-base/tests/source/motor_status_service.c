#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "motor/identity.h"
#include "motor/status_service.h"
#include "platform/aux_bus.h"

static PlatformAuxBusStatus bus_status;
static uint16_t requested_register;
static const uint8_t *requested_write_data;
static uint8_t *requested_read_data;
static uint16_t requested_length;
static bool requested_read;
static uint8_t start_count;

bool platform_aux_bus_start_write(uint8_t address, uint16_t register_address, const uint8_t *data,
                                  uint16_t length) {
    assert(address == 0x78);
    assert(bus_status == PLATFORM_AUX_BUS_IDLE);
    requested_register = register_address;
    requested_write_data = data;
    requested_read_data = 0;
    requested_length = length;
    requested_read = false;
    start_count++;
    bus_status = PLATFORM_AUX_BUS_BUSY;
    return true;
}

bool platform_aux_bus_start_read(uint8_t address, uint16_t register_address, uint8_t *data,
                                 uint16_t length) {
    assert(address == 0x78);
    assert(bus_status == PLATFORM_AUX_BUS_IDLE);
    requested_register = register_address;
    requested_write_data = 0;
    requested_read_data = data;
    requested_length = length;
    requested_read = true;
    start_count++;
    bus_status = PLATFORM_AUX_BUS_BUSY;
    return true;
}

PlatformAuxBusStatus platform_aux_bus_status(void) { return bus_status; }

void platform_aux_bus_clear(void) { bus_status = PLATFORM_AUX_BUS_IDLE; }

static void reset_bus(void) {
    bus_status = PLATFORM_AUX_BUS_IDLE;
    requested_register = 0;
    requested_write_data = 0;
    requested_read_data = 0;
    requested_length = 0;
    requested_read = false;
    start_count = 0;
}

static MotorIdentity identity(MotorProtocol protocol) {
    MotorIdentity value = {0};
    value.protocol = protocol;
    return value;
}

static void finish(uint8_t response) {
    if (requested_read) {
        *requested_read_data = response;
    }
    bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
}

static void test_extended_status_cycle(void) {
    MotorStatusService service;
    MotorIdentity extended = identity(MOTOR_PROTOCOL_POSITION_A);
    reset_bus();
    motor_status_service_init(&service, &extended);

    motor_status_service_run(&service, 0);
    assert(!requested_read);
    assert(requested_register == 4);
    assert(requested_length == 1);
    assert(*requested_write_data == 0);

    finish(0);
    motor_status_service_run(&service, 1);
    assert(requested_read);
    assert(start_count == 2);

    finish(0xaa);
    motor_status_service_run(&service, 2);
    assert(motor_status_service_output_inhibited(&service));

    motor_status_service_run(&service, 201);
    assert(start_count == 2);
    motor_status_service_run(&service, 202);
    assert(start_count == 3);
    assert(requested_read);
}

static void test_standard_status_response(void) {
    MotorStatusService service;
    MotorIdentity standard = identity(MOTOR_PROTOCOL_STANDARD);
    reset_bus();
    motor_status_service_init(&service, &standard);

    motor_status_service_run(&service, 0);
    finish(0);
    motor_status_service_run(&service, 1);
    finish(0xff);
    motor_status_service_run(&service, 2);

    assert(motor_status_service_output_inhibited(&service));
}

static void test_legacy_status_disabled(void) {
    MotorStatusService service;
    MotorIdentity legacy = identity(MOTOR_PROTOCOL_LEGACY);
    reset_bus();
    motor_status_service_init(&service, &legacy);

    motor_status_service_run(&service, 0);

    assert(service.phase == MOTOR_STATUS_DISABLED);
    assert(start_count == 0);
    assert(!motor_status_service_output_inhibited(&service));
}

static void test_failed_transfer_retries(void) {
    MotorStatusService service;
    MotorIdentity standard = identity(MOTOR_PROTOCOL_STANDARD);
    reset_bus();
    motor_status_service_init(&service, &standard);

    motor_status_service_run(&service, 0);
    bus_status = PLATFORM_AUX_BUS_FAILED;
    motor_status_service_run(&service, 1);

    assert(service.phase == MOTOR_STATUS_INITIALIZE);
    assert(start_count == 2);
    assert(!requested_read);
}

int main(void) {
    test_extended_status_cycle();
    test_standard_status_response();
    test_legacy_status_disabled();
    test_failed_transfer_retries();
    return 0;
}
