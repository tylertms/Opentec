#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "motor/startup_centering.h"
#include "platform/aux_bus.h"

static PlatformAuxBusStatus bus_status;
static uint8_t requested_address;
static uint16_t requested_register;
static uint8_t *requested_data;
static uint16_t requested_length;
static uint8_t requested_write_value;
static uint8_t start_count;

bool platform_aux_bus_start_write(uint8_t address, uint16_t register_address, const uint8_t *data,
                                  uint16_t length) {
    if (bus_status != PLATFORM_AUX_BUS_IDLE) {
        return false;
    }
    requested_address = address;
    requested_register = register_address;
    requested_length = length;
    requested_write_value = data[0];
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
    requested_data = data;
    requested_length = length;
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
    requested_data = 0;
    requested_length = 0;
    requested_write_value = 0;
    start_count = 0;
}

static void finish_read(uint8_t value, PlatformAuxBusStatus status) {
    *requested_data = value;
    bus_status = status;
}

static void test_waits_for_controller_readiness(void) {
    MotorStartupCentering centering;
    reset_bus();
    motor_startup_centering_init(&centering, 100, false);

    assert(motor_startup_centering_run(&centering, 100, true, 1000) == 0);
    assert(requested_address == 0x78);
    assert(requested_register == 8);
    assert(requested_length == 1);
    assert(start_count == 1);

    finish_read(0, PLATFORM_AUX_BUS_SUCCEEDED);
    assert(motor_startup_centering_run(&centering, 101, true, 1000) == 0);
    assert(start_count == 2);

    finish_read(1, PLATFORM_AUX_BUS_SUCCEEDED);
    assert(motor_startup_centering_run(&centering, 102, true, 1000) == 0);
    assert(motor_startup_centering_active(&centering));
}

static void test_retries_failed_readiness_reads(void) {
    MotorStartupCentering centering;
    reset_bus();
    motor_startup_centering_init(&centering, 0, false);

    assert(motor_startup_centering_run(&centering, 0, false, 0) == 0);
    finish_read(0, PLATFORM_AUX_BUS_FAILED);
    assert(motor_startup_centering_run(&centering, 1, false, 0) == 0);
    assert(start_count == 2);
    assert(!motor_startup_centering_active(&centering));
}

static void test_readiness_timeout_is_exact_and_wrap_safe(void) {
    MotorStartupCentering centering;
    reset_bus();
    motor_startup_centering_init(&centering, UINT32_MAX - 2500, false);

    assert(motor_startup_centering_run(&centering, UINT32_MAX - 2500, false, 0) == 0);
    bus_status = PLATFORM_AUX_BUS_BUSY;
    assert(motor_startup_centering_run(&centering, 2498, false, 0) == 0);
    assert(!motor_startup_centering_complete(&centering));
    finish_read(0, PLATFORM_AUX_BUS_SUCCEEDED);
    assert(motor_startup_centering_run(&centering, 2499, false, 0) == 0);
    assert(motor_startup_centering_complete(&centering));
}

static void test_applies_the_elapsed_force_envelope(void) {
    MotorStartupCentering centering;
    reset_bus();
    motor_startup_centering_init(&centering, 0, false);
    assert(motor_startup_centering_run(&centering, 0, true, 1000) == 0);
    finish_read(1, PLATFORM_AUX_BUS_SUCCEEDED);
    assert(motor_startup_centering_run(&centering, 100, true, 1000) == 0);

    assert(motor_startup_centering_run(&centering, 499, true, 1000) == -399);
    assert(motor_startup_centering_run(&centering, 500, true, 1000) == -800);
    assert(motor_startup_centering_run(&centering, 4100, true, 5000) == -44000);
    assert(motor_startup_centering_complete(&centering));
}

static void test_preserves_direction_and_handles_missing_position(void) {
    MotorStartupCentering centering;
    reset_bus();
    motor_startup_centering_init(&centering, 0, false);
    assert(motor_startup_centering_run(&centering, 0, false, 0) == 0);
    finish_read(1, PLATFORM_AUX_BUS_SUCCEEDED);
    assert(motor_startup_centering_run(&centering, 1, false, 0) == 0);
    assert(motor_startup_centering_run(&centering, 401, false, -1000) == 0);
    assert(motor_startup_centering_run(&centering, 401, true, -1000) == 800);
}

static void test_prepares_damping_before_readiness(void) {
    MotorStartupCentering centering;
    reset_bus();
    motor_startup_centering_init(&centering, 100, true);

    assert(motor_startup_centering_run(&centering, 100, false, 0) == 0);
    assert(requested_address == 0x78);
    assert(requested_register == 0x23);
    assert(requested_length == 1);
    assert(requested_write_value == 0xff);

    bus_status = PLATFORM_AUX_BUS_FAILED;
    assert(motor_startup_centering_run(&centering, 101, false, 0) == 0);
    assert(start_count == 2);
    assert(requested_register == 0x23);

    bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
    assert(motor_startup_centering_run(&centering, 200, false, 0) == 0);
    assert(start_count == 3);
    assert(requested_register == 8);

    assert(motor_startup_centering_run(&centering, 5199, false, 0) == 0);
    assert(!motor_startup_centering_complete(&centering));
    finish_read(0, PLATFORM_AUX_BUS_SUCCEEDED);
    assert(motor_startup_centering_run(&centering, 5200, false, 0) == 0);
    assert(motor_startup_centering_complete(&centering));
}

int main(void) {
    test_waits_for_controller_readiness();
    test_retries_failed_readiness_reads();
    test_readiness_timeout_is_exact_and_wrap_safe();
    test_applies_the_elapsed_force_envelope();
    test_preserves_direction_and_handles_missing_position();
    test_prepares_damping_before_readiness();
    return 0;
}
