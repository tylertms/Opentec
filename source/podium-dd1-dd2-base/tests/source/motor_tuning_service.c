#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "motor/tuning_service.h"
#include "platform/aux_bus.h"

static PlatformAuxBusStatus bus_status;
static uint8_t bus_address;
static uint16_t register_address;
static uint8_t transfer_data[2];
static uint16_t transfer_length;
static uint8_t start_count;

bool platform_aux_bus_start_write(uint8_t address, uint16_t address_register, const uint8_t *data,
                                  uint16_t length) {
    if (bus_status != PLATFORM_AUX_BUS_IDLE) {
        return false;
    }

    bus_address = address;
    register_address = address_register;
    transfer_length = length;
    transfer_data[0] = data[0];
    transfer_data[1] = length > 1 ? data[1] : 0;
    start_count++;
    bus_status = PLATFORM_AUX_BUS_BUSY;
    return true;
}

PlatformAuxBusStatus platform_aux_bus_status(void) { return bus_status; }

void platform_aux_bus_clear(void) { bus_status = PLATFORM_AUX_BUS_IDLE; }

static MotorTuningContext default_context(void) {
    return (MotorTuningContext){
        .automatic_rotation_degrees = 1080,
        .ramp_percent = 100,
        .strength_percent = 100,
    };
}

static void reset_bus(void) {
    bus_status = PLATFORM_AUX_BUS_IDLE;
    bus_address = 0;
    register_address = 0;
    transfer_data[0] = 0;
    transfer_data[1] = 0;
    transfer_length = 0;
    start_count = 0;
}

static void test_writes_parameters_in_order(void) {
    MotorTuningService service;
    TuningProfile profile;
    MotorTuningContext context = default_context();
    tuning_profile_defaults(&profile);
    reset_bus();
    motor_tuning_service_init(&service, &profile, &context);

    motor_tuning_service_run(&service);
    assert(bus_address == 0x78);
    assert(register_address == 0x20);
    assert(transfer_length == 1);
    assert(start_count == 1);

    motor_tuning_service_run(&service);
    assert(start_count == 1);

    bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
    motor_tuning_service_run(&service);
    assert(register_address == 0x21);
    assert(transfer_data[0] == profile.force_feedback_strength);
    assert(start_count == 2);
}

static void test_retries_failed_parameter(void) {
    MotorTuningService service;
    TuningProfile profile;
    MotorTuningContext context = default_context();
    tuning_profile_defaults(&profile);
    reset_bus();
    motor_tuning_service_init(&service, &profile, &context);

    motor_tuning_service_run(&service);
    bus_status = PLATFORM_AUX_BUS_FAILED;
    motor_tuning_service_run(&service);

    assert(register_address == 0x20);
    assert(start_count == 2);
}

static void test_waits_for_other_bus_owner(void) {
    MotorTuningService service;
    TuningProfile profile;
    MotorTuningContext context = default_context();
    tuning_profile_defaults(&profile);
    reset_bus();
    bus_status = PLATFORM_AUX_BUS_BUSY;
    motor_tuning_service_init(&service, &profile, &context);

    motor_tuning_service_run(&service);
    assert(start_count == 0);
    assert(motor_tuning_service_pending(&service));
}

int main(void) {
    test_writes_parameters_in_order();
    test_retries_failed_parameter();
    test_waits_for_other_bus_owner();
    return 0;
}
