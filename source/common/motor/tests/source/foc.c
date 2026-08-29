#include "common/motor/foc.h"

#include <assert.h>
#include <limits.h>

static MotorFocInput motor_foc_test_input(void) {
    return (MotorFocInput){
        .dc_bus_voltage = INT16_MAX,
        .rotor_sin_cos = {.f16Cos = INT16_MAX},
    };
}

static void test_current_error_saturation(void) {
    MotorFocState state;
    MotorFocOutput output;
    motor_foc_initialize(&state);

    MotorFocInput input = motor_foc_test_input();
    input.current_reference.f16D = INT16_MAX;
    input.phase_current.f16A = -1;
    motor_foc_step(&state, &input, &output);
    assert(state.d_controller.f16InErrK_1 == INT16_MAX);

    input = motor_foc_test_input();
    input.current_reference.f16Q = INT16_MIN;
    input.phase_current.f16B = 1;
    motor_foc_step(&state, &input, &output);
    assert(state.q_controller.f16InErrK_1 == INT16_MIN);
}

static void test_saturated_voltage_limits(void) {
    MotorFocState state;
    MotorFocOutput output;
    motor_foc_initialize(&state);

    MotorFocInput input = motor_foc_test_input();
    input.dc_bus_voltage = INT16_MIN;
    motor_foc_step(&state, &input, &output);
    assert(state.d_controller.f16LowerLim == INT16_MAX);
}

int main(void) {
    test_current_error_saturation();
    test_saturated_voltage_limits();
    return 0;
}
