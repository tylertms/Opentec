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

static void test_natural_friction(void) {
    MotorDriveFrictionState state;
    motor_drive_friction_initialize(&state, 0x0020e374U);
    assert(state.excursion_limit == 3288U);
    assert(state.output_scale == 9U);
    assert(motor_drive_friction_step(&state, 500, 0U) == 0);
    assert(state.anchor_position == 0);

    assert(motor_drive_friction_step(&state, 1000, UINT16_MAX) == 2952);
    assert(state.anchor_position == 672);
    assert(motor_drive_friction_step(&state, 900, UINT16_MAX) == 2052);
    assert(motor_drive_friction_step(&state, 600, UINT16_MAX) == 0);
    assert(motor_drive_friction_step(&state, 500, UINT16_MAX) == -1548);
    assert(motor_drive_friction_step(&state, 0, UINT16_MAX) == -2952);
    assert(state.anchor_position == 328);

    MotorDriveFrictionState alternate;
    motor_drive_friction_initialize(&alternate, 0x002120a3U);
    assert(alternate.excursion_limit == 3312U);
    assert(alternate.output_scale == 9U);
}

static void test_product_derating(void) {
    MotorDriveDeratingState dd1;
    motor_drive_derating_initialize(&dd1, 0x5999);
    assert(motor_drive_product_scale(&dd1, 30000, 2000U, 0x5999, 0x4000, false) == 20998);
    assert(dd1.target_scale == 18322);
    assert(dd1.error == -4615);
    assert(motor_drive_product_scale(&dd1, 10000, 0U, 0x5999, 0x4000, false) == 6998);
    assert(dd1.target_scale == 0x4000);
    assert(dd1.error == -6553);
    dd1.current_scale = 0x4000;
    assert(motor_drive_product_scale(&dd1, 20000, 2000U, 0x5999, 0x4000, false) == 9999);
    assert(motor_drive_product_scale(&dd1, 20000, 2000U, 0x5999, 0x4000, true) == 10000);

    MotorDriveDeratingState dd2;
    motor_drive_derating_initialize(&dd2, 0x770a);
    assert(motor_drive_product_scale(&dd2, 30000, 2000U, 0x770a, 0x5c28, false) == 27898);
    assert(dd2.target_scale == 26167);
    assert(dd2.error == -4307);
    assert(motor_drive_product_scale(&dd2, 20000, 2000U, 0x770a, 0x5c28, true) == 14399);
}

static void test_overspeed_latch(void) {
    MotorDriveOverspeedState state = {0};
    assert(motor_drive_overspeed_apply(&state, 1234, 0x2ccc) == 1234);
    assert(!state.latched);
    assert(motor_drive_overspeed_apply(&state, 1234, 0x2ccd) == 1234);
    assert(state.latched);
    assert(motor_drive_overspeed_apply(&state, -1234, 0) == 0);

    state = (MotorDriveOverspeedState){0};
    assert(motor_drive_overspeed_apply(&state, -1234, -0x2ccd) == -1234);
    assert(state.latched);
    assert(motor_drive_overspeed_apply(&state, 1234, 0) == 0);
}

int main(void) {
    test_normal_product_scales();
    test_full_torque_and_gates();
    test_interpolation_filter();
    test_natural_motion_resistance();
    test_natural_friction();
    test_product_derating();
    test_overspeed_latch();
    return 0;
}
