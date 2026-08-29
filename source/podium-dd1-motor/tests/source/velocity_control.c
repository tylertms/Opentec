#include "motor/velocity_control.h"

#include <assert.h>
#include <limits.h>

static void test_initialize_and_step(void) {
    MotorVelocityControlState state;
    motor_velocity_control_initialize(&state, 0x5999);

    assert(state.controller.a32PGain == 0x14000);
    assert(state.controller.a32IGain == 0x617);
    assert(state.controller.f16UpperLim == 0x5999);
    assert(state.controller.f16LowerLim == -0x5999);
    assert(state.target_ramp.f32RampUp == INT32_MAX);
    assert(state.target_ramp.f32RampDown == INT32_MAX);

    motor_velocity_control_target_set(&state, 327);
    assert(motor_velocity_control_step(&state, 100, 0U) == 227);
    assert(state.ramped_velocity == 327);
    assert(state.velocity_error == 227);
    assert(state.stop_integrator == 0U);

    motor_velocity_control_target_set(&state, INT16_MAX);
    assert(motor_velocity_control_step(&state, INT16_MIN, 1U) == 0x5999);
    assert(state.velocity_error == INT16_MAX);
    assert(state.stop_integrator == 1U);
}

static void test_reset(void) {
    MotorVelocityControlState state;
    motor_velocity_control_initialize(&state, 0x770a);
    motor_velocity_control_target_set(&state, -327);
    (void)motor_velocity_control_step(&state, 100, 0U);

    motor_velocity_control_reset(&state);

    assert(state.target_velocity == 0);
    assert(state.ramped_velocity == 0);
    assert(state.velocity_error == 0);
    assert(state.current_reference == 0);
    assert(state.stop_integrator == 0U);
    assert(state.controller.f16UpperLim == 0x770a);
    assert(state.controller.f16LowerLim == -0x770a);
}

int main(void) {
    test_initialize_and_step();
    test_reset();
    return 0;
}
