#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "motor/output_status.h"
#include "motor/output_transport.h"

static void test_initializes_with_all_flags_clear(void) {
    MotorOutputStatus status = {.value = UINT8_MAX};

    motor_output_status_init(&status);

    assert(status.value == 0);
}

static void test_maps_normal_mode_conditions(void) {
    MotorOutputStatus status;
    motor_output_status_init(&status);
    MotorOutputStatusInput input = {
        .force_enabled = true,
        .override_active = true,
        .transition_active = true,
        .primary_disabled = true,
        .secondary_disabled = true,
        .usb_disconnected = true,
        .full_torque = true,
    };

    assert(motor_output_status_update(&status, &input) == 0xf7);
}

static void test_xbox_mode_retains_motor_gates(void) {
    MotorOutputStatus status = {
        .value = MOTOR_OUTPUT_STATUS_ENABLED | MOTOR_OUTPUT_STATUS_OVERRIDE_ACTIVE |
                 MOTOR_OUTPUT_STATUS_TRANSITION_ACTIVE | MOTOR_OUTPUT_STATUS_USB_DISCONNECTED,
    };
    MotorOutputStatusInput input = {
        .xbox_mode = true,
        .primary_disabled = true,
        .full_torque = true,
    };

    uint8_t value = motor_output_status_update(&status, &input);

    assert((value & MOTOR_OUTPUT_STATUS_REMOTE_EFFECTS) == 0);
    assert((value & MOTOR_OUTPUT_STATUS_ENABLED) != 0);
    assert((value & MOTOR_OUTPUT_STATUS_OVERRIDE_ACTIVE) != 0);
    assert((value & MOTOR_OUTPUT_STATUS_TRANSITION_ACTIVE) != 0);
    assert((value & MOTOR_OUTPUT_STATUS_PRIMARY_DISABLED) != 0);
    assert((value & MOTOR_OUTPUT_STATUS_SECONDARY_DISABLED) == 0);
    assert((value & MOTOR_OUTPUT_STATUS_USB_DISCONNECTED) != 0);
    assert((value & MOTOR_OUTPUT_STATUS_FULL_TORQUE) != 0);
}

static void test_leaving_xbox_refreshes_motor_gates(void) {
    MotorOutputStatus status = {.value = UINT8_MAX};
    MotorOutputStatusInput input = {0};

    uint8_t value = motor_output_status_update(&status, &input);

    assert(value == MOTOR_OUTPUT_STATUS_REMOTE_EFFECTS);
}

static void test_non_xbox_direct_force_refreshes_motor_gates(void) {
    MotorOutputStatus status = {.value = UINT8_MAX};
    MotorOutputStatusInput input = {
        .direct_force = true,
        .force_enabled = true,
    };

    uint8_t value = motor_output_status_update(&status, &input);

    assert(value == (MOTOR_OUTPUT_STATUS_REMOTE_EFFECTS | MOTOR_OUTPUT_STATUS_ENABLED));
}

int main(void) {
    test_initializes_with_all_flags_clear();
    test_maps_normal_mode_conditions();
    test_xbox_mode_retains_motor_gates();
    test_leaving_xbox_refreshes_motor_gates();
    test_non_xbox_direct_force_refreshes_motor_gates();
    return 0;
}
