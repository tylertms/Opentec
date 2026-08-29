#include "common/motor/drive.h"

#include <assert.h>

static void test_normal_product_scales(void) {
    MotorDriveCommand dd1 =
        motor_drive_command_resolve(true, 65535U, 1000, 53U, false, false, false);
    assert(dd1.primary_current == 17366);
    assert(dd1.secondary_current == 530);
    assert(dd1.controller_coefficient == 0x9999U);
    assert(dd1.controller_scale == 0x147U);

    MotorDriveCommand dd2 =
        motor_drive_command_resolve(false, 65535U, -1000, 40U, false, false, false);
    assert(dd2.primary_current == -13107);
    assert(dd2.secondary_current == -400);
}

static void test_full_torque_and_gates(void) {
    MotorDriveCommand command =
        motor_drive_command_resolve(true, 70000U, 70000, 40U, true, false, false);
    assert(command.primary_current == 32767);
    assert(command.secondary_current == -1);
    assert(command.controller_coefficient == 0x9999U);

    command = motor_drive_command_resolve(true, 0U, 1234, 53U, true, false, true);
    assert(command.primary_current == 0);
    assert(command.secondary_current == 0);
    assert(command.controller_coefficient == 0x11c7U);

    command = motor_drive_command_resolve(true, 1000U, 0, 53U, true, true, false);
    assert(command.controller_coefficient == 0x11c7U);
}

static void test_interpolation_filter(void) {
    MotorDriveInterpolationState state = {0};
    assert(motor_drive_interpolation_step(&state, 10000, 6U) == 289);
    assert(state.error == 10000);
    assert(state.accumulator == 29570U);
    assert(motor_drive_interpolation_step(&state, 10000, 6U) == 580);
    assert(state.error == 9701);
    assert(state.accumulator == 58251U);

    assert(motor_drive_interpolation_step(&state, -1234, 20U) == -1234);
    assert(state.output == -1234);
    assert(state.error == 0);
    assert(state.accumulator == 0U);
}

static void test_natural_motion_resistance(void) {
    assert(motor_drive_motion_resistance_resolve(1000, 255U) == 2988);
    assert(motor_drive_motion_resistance_resolve(-1000, 255U) == -2991);
    assert(motor_drive_motion_resistance_resolve(INT16_MAX, 255U) == INT16_MAX);
    assert(motor_drive_motion_resistance_resolve(1000, 0U) == 0);
}

int main(void) {
    test_normal_product_scales();
    test_full_torque_and_gates();
    test_interpolation_filter();
    test_natural_motion_resistance();
    return 0;
}
