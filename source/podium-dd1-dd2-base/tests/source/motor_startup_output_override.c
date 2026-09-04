#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "motor/startup_output_override.h"
#include "platform/aux_bus.h"

static PlatformAuxBusStatus bus_status;
static PlatformAuxBusStatus completion_status;
static bool start_allowed;
static uint8_t busy_cycles_before_completion;
static uint8_t requested_address;
static uint16_t requested_register;
static uint8_t requested_value;
static uint16_t requested_length;
static uint8_t start_count;
static uint8_t service_count;

bool platform_aux_bus_start_write(uint8_t address, uint16_t register_address, const uint8_t *data,
                                  uint16_t length) {
    start_count++;
    if (!start_allowed || bus_status != PLATFORM_AUX_BUS_IDLE) {
        return false;
    }
    requested_address = address;
    requested_register = register_address;
    requested_value = data[0];
    requested_length = length;
    bus_status = PLATFORM_AUX_BUS_BUSY;
    return true;
}

PlatformAuxBusStatus platform_aux_bus_status(void) { return bus_status; }

void platform_aux_bus_clear(void) {
    if (bus_status != PLATFORM_AUX_BUS_BUSY) {
        bus_status = PLATFORM_AUX_BUS_IDLE;
    }
}

void platform_aux_bus_service(void) {
    service_count++;
    if (bus_status == PLATFORM_AUX_BUS_BUSY) {
        if (busy_cycles_before_completion != 0) {
            busy_cycles_before_completion--;
            return;
        }
        bus_status = completion_status;
    }
}

static void reset_bus(void) {
    bus_status = PLATFORM_AUX_BUS_IDLE;
    completion_status = PLATFORM_AUX_BUS_SUCCEEDED;
    start_allowed = true;
    busy_cycles_before_completion = 0;
    requested_address = 0;
    requested_register = 0;
    requested_value = 0;
    requested_length = 0;
    start_count = 0;
    service_count = 0;
}

static MotorIdentity standard_identity(void) {
    return (MotorIdentity){.protocol = MOTOR_PROTOCOL_STANDARD};
}

static void test_refused_start_is_not_retried(void) {
    MotorIdentity identity = standard_identity();
    reset_bus();
    start_allowed = false;

    assert(!motor_startup_output_override_write(&identity));
    assert(start_count == 1);
    assert(service_count == 0);
    assert(bus_status == PLATFORM_AUX_BUS_IDLE);
}

static void test_terminal_failure_is_completion(void) {
    MotorIdentity identity = standard_identity();
    reset_bus();
    completion_status = PLATFORM_AUX_BUS_FAILED;

    assert(motor_startup_output_override_write(&identity));
    assert(start_count == 1);
    assert(service_count == 1);
    assert(bus_status == PLATFORM_AUX_BUS_IDLE);
}

static void test_waits_for_previous_busy_transfer_before_starting(void) {
    MotorIdentity identity = standard_identity();
    reset_bus();
    bus_status = PLATFORM_AUX_BUS_BUSY;

    assert(motor_startup_output_override_write(&identity));
    assert(start_count == 1);
    assert(service_count == 2);
    assert(bus_status == PLATFORM_AUX_BUS_IDLE);
}

static void test_waits_for_completion_without_timeout(void) {
    MotorIdentity identity = standard_identity();
    reset_bus();
    busy_cycles_before_completion = 3;

    assert(motor_startup_output_override_write(&identity));
    assert(start_count == 1);
    assert(service_count == 4);
    assert(bus_status == PLATFORM_AUX_BUS_IDLE);
}

static void test_successful_transfer_writes_the_override_once(void) {
    MotorIdentity identity = standard_identity();
    reset_bus();

    assert(motor_startup_output_override_write(&identity));
    assert(start_count == 1);
    assert(service_count == 1);
    assert(requested_address == 0x78);
    assert(requested_register == 0x23);
    assert(requested_length == 1);
    assert(requested_value == 0xff);
    assert(bus_status == PLATFORM_AUX_BUS_IDLE);
}

static void test_legacy_controller_is_not_started(void) {
    MotorIdentity identity = {.protocol = MOTOR_PROTOCOL_LEGACY};
    reset_bus();

    assert(!motor_startup_output_override_write(&identity));
    assert(!motor_startup_output_override_write(NULL));
    assert(start_count == 0);
    assert(service_count == 0);
}

int main(void) {
    test_refused_start_is_not_retried();
    test_terminal_failure_is_completion();
    test_waits_for_previous_busy_transfer_before_starting();
    test_waits_for_completion_without_timeout();
    test_successful_transfer_writes_the_override_once();
    test_legacy_controller_is_not_started();
    return 0;
}
