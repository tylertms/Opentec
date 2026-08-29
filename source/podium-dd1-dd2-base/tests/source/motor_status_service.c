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

static void finish_status(uint8_t response) {
    if (requested_read) {
        *requested_read_data = response;
    }
    bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
}

static void finish_command(uint16_t response) {
    assert(requested_read);
    assert(requested_length == 2);
    requested_read_data[0] = (uint8_t)response;
    requested_read_data[1] = (uint8_t)(response >> 8);
    bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
}

static void finish_write(void) { bus_status = PLATFORM_AUX_BUS_SUCCEEDED; }

static void test_extended_status_cycle(void) {
    MotorStatusService service;
    MotorIdentity extended = identity(MOTOR_PROTOCOL_POSITION);
    reset_bus();
    motor_status_service_init(&service, &extended);

    motor_status_service_run(&service, 0);
    assert(requested_read);
    assert(requested_register == 5);
    assert(requested_length == 2);

    finish_command(0xaaaa);
    motor_status_service_run(&service, 1);
    assert(!requested_read);
    assert(requested_register == 4);
    assert(requested_length == 1);
    assert(*requested_write_data == 0);
    assert(start_count == 2);

    finish_write();
    motor_status_service_run(&service, 2);
    assert(requested_read);
    assert(requested_register == 4);

    finish_status(0xaa);
    motor_status_service_run(&service, 3);
    assert(motor_status_service_output_inhibited(&service));

    motor_status_service_run(&service, 202);
    assert(start_count == 3);
    motor_status_service_run(&service, 203);
    assert(start_count == 4);
    assert(requested_read);
    assert(requested_register == 5);
}

static void test_standard_status_response(void) {
    MotorStatusService service;
    MotorIdentity standard = identity(MOTOR_PROTOCOL_STANDARD);
    reset_bus();
    motor_status_service_init(&service, &standard);

    motor_status_service_run(&service, 0);
    finish_write();
    motor_status_service_run(&service, 1);
    finish_status(0xff);
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
    MotorIdentity extended = identity(MOTOR_PROTOCOL_POSITION);
    reset_bus();
    motor_status_service_init(&service, &extended);

    motor_status_service_run(&service, 0);
    bus_status = PLATFORM_AUX_BUS_FAILED;
    motor_status_service_run(&service, 1);

    assert(service.phase == MOTOR_STATUS_READ_COMMAND);
    assert(start_count == 2);
    assert(requested_read);
    assert(requested_register == 5);
}

static void finish_initial_status_cycle(MotorStatusService *service, uint32_t now_ms) {
    finish_write();
    motor_status_service_run(service, now_ms);
    finish_status(0);
    motor_status_service_run(service, now_ms + 1);
}

static void test_extended_command_request_and_acknowledgement(void) {
    MotorStatusService service;
    MotorIdentity extended = identity(MOTOR_PROTOCOL_POSITION);
    reset_bus();
    motor_status_service_init(&service, &extended);
    motor_status_service_request_command(&service);

    motor_status_service_run(&service, 0);
    finish_command(0);
    motor_status_service_run(&service, 1);

    assert(motor_status_service_take_event(&service) ==
           MOTOR_STATUS_EVENT_POSITION_SENSOR_TEST_FAILED);
    assert(motor_status_service_take_event(&service) == MOTOR_STATUS_EVENT_NONE);
    assert(!requested_read);
    assert(requested_register == 5);
    assert(requested_length == 2);
    assert(requested_write_data[0] == 0xcd);
    assert(requested_write_data[1] == 0xab);
    assert(service.command_sent);

    finish_write();
    motor_status_service_run(&service, 2);
    finish_initial_status_cycle(&service, 3);
    motor_status_service_run(&service, 204);
    finish_command(0);
    motor_status_service_run(&service, 205);

    assert(motor_status_service_take_event(&service) ==
           MOTOR_STATUS_EVENT_POSITION_SENSOR_TEST_STARTED);
    assert(motor_status_service_take_event(&service) == MOTOR_STATUS_EVENT_NONE);
    assert(!service.command_pending);
    assert(!service.command_sent);
}

static void test_extended_command_fault_latches_output(void) {
    MotorStatusService service;
    MotorIdentity extended = identity(MOTOR_PROTOCOL_POSITION);
    reset_bus();
    motor_status_service_init(&service, &extended);
    motor_status_service_request_command(&service);

    motor_status_service_run(&service, 0);
    finish_command(0xbbbb);
    motor_status_service_run(&service, 1);

    assert(motor_status_service_take_event(&service) == MOTOR_STATUS_EVENT_TORQUE_REDUCED);
    assert(motor_status_service_take_event(&service) == MOTOR_STATUS_EVENT_NONE);
    assert(motor_status_service_output_inhibited(&service));
    assert(!service.command_pending);
    assert(!service.command_sent);
}

static void test_terminal_command_responses_preserve_request(void) {
    static const uint16_t responses[] = {0xaaaa, 0xffff};
    MotorIdentity extended = identity(MOTOR_PROTOCOL_POSITION);

    for (size_t index = 0; index < sizeof(responses) / sizeof(responses[0]); index++) {
        MotorStatusService service;
        reset_bus();
        motor_status_service_init(&service, &extended);
        motor_status_service_request_command(&service);

        motor_status_service_run(&service, 0);
        finish_command(responses[index]);
        motor_status_service_run(&service, 1);

        assert(service.command_pending);
        assert(!service.command_sent);
        assert(motor_status_service_take_event(&service) == MOTOR_STATUS_EVENT_NONE);
        assert(service.phase == MOTOR_STATUS_INITIALIZE);
    }
}

static void test_unknown_command_response_retries_read(void) {
    MotorStatusService service;
    MotorIdentity extended = identity(MOTOR_PROTOCOL_POSITION);
    reset_bus();
    motor_status_service_init(&service, &extended);

    motor_status_service_run(&service, 0);
    finish_command(0x1234);
    motor_status_service_run(&service, 1);

    assert(start_count == 2);
    assert(requested_read);
    assert(requested_register == 5);
}

int main(void) {
    test_extended_status_cycle();
    test_standard_status_response();
    test_legacy_status_disabled();
    test_failed_transfer_retries();
    test_extended_command_request_and_acknowledgement();
    test_extended_command_fault_latches_output();
    test_terminal_command_responses_preserve_request();
    test_unknown_command_response_retries_read();
    return 0;
}
