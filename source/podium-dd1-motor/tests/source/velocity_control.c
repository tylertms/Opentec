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

static void test_current_limiter_overspeed_gate(void) {
    assert_stop_integrator(300, 400, 1, 0, 0);
    assert_stop_integrator(-300, -400, 1, 0, 0);
    assert_stop_integrator(300, 300, 1, 0, 1);
    assert_stop_integrator(-300, -300, 1, 0, 1);
}

static void test_velocity_limiter_overspeed_gate(void) {
    assert_stop_integrator(300, 400, 0, 1, 0);
    assert_stop_integrator(-300, -400, 0, 1, 0);
    assert_stop_integrator(300, 300, 0, 1, 1);
    assert_stop_integrator(-300, -300, 0, 1, 1);
}

static void test_saturated_absolute_gate(void) {
    assert_stop_integrator(INT16_MIN, INT16_MAX, 1, 0, 1);
    assert_stop_integrator(INT16_MIN, INT16_MAX, 0, 1, 1);
}

int main(void) {
    test_current_limiter_overspeed_gate();
    test_velocity_limiter_overspeed_gate();
    test_saturated_absolute_gate();
    return 0;
}
