#include "motor/velocity_control.h"

#include <assert.h>
#include <stdint.h>

static void assert_stop_integrator(int16_t target_velocity, int16_t measured_velocity,
                                   bool_t current_controller_limited,
                                   bool_t velocity_controller_limited, bool_t expected) {
    MotorVelocityControlState state;

    motor_velocity_control_initialize(&state, INT16_MAX);
    motor_velocity_control_target_set(&state, target_velocity);
    state.controller.bLimFlag = velocity_controller_limited;
    motor_velocity_control_step(&state, measured_velocity, current_controller_limited);

    assert(state.stop_integrator == expected);
}

static void test_current_limiter_stops_integration(void) {
    assert_stop_integrator(300, 400, 1, 0, 1);
    assert_stop_integrator(-300, -400, 1, 0, 1);
    assert_stop_integrator(300, 300, 1, 0, 1);
    assert_stop_integrator(-300, -300, 1, 0, 1);
}

static void test_velocity_limiter_stops_integration(void) {
    assert_stop_integrator(300, 400, 0, 1, 1);
    assert_stop_integrator(-300, -400, 0, 1, 1);
    assert_stop_integrator(300, 300, 0, 1, 1);
    assert_stop_integrator(-300, -300, 0, 1, 1);
}

static void test_saturated_absolute_gate(void) {
    assert_stop_integrator(INT16_MIN, INT16_MAX, 1, 0, 1);
    assert_stop_integrator(INT16_MIN, INT16_MAX, 0, 1, 1);
}

static void test_reset_clears_runtime_history(void) {
    MotorVelocityControlState state;
    motor_velocity_control_initialize(&state, INT16_MAX);
    state.target_velocity = 123;
    state.target_ramp.f32State = INT32_C(0x12345678);
    state.ramped_velocity = 234;
    state.velocity_error = -345;
    state.current_reference = 456;
    state.stop_integrator = 1U;
    state.controller.f32IAccK_1 = INT32_C(0x23456789);
    state.controller.f16InErrK_1 = -567;
    state.controller.bLimFlag = 1U;

    motor_velocity_control_reset(&state);

    assert(state.target_velocity == 0);
    assert(state.target_ramp.f32State == 0);
    assert(state.ramped_velocity == 0);
    assert(state.velocity_error == 0);
    assert(state.current_reference == 0);
    assert(state.stop_integrator == 0U);
    assert(state.controller.f32IAccK_1 == 0);
    assert(state.controller.f16InErrK_1 == 0);
    assert(state.controller.bLimFlag == 0U);
}

int motor_test_velocity_control(void) {
    test_current_limiter_stops_integration();
    test_velocity_limiter_stops_integration();
    test_saturated_absolute_gate();
    test_reset_clears_runtime_history();
    return 0;
}
