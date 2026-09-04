#include "motor/foc.h"

#include <assert.h>
#include <stdint.h>

static void test_zero_bus_runs_limited_control_cycle(void) {
    MotorFocState state;
    MotorFocInput input = {0};
    MotorFocOutput output = {0};

    motor_foc_initialize(&state);
    state.d_controller.f32IAccK_1 = 1;
    state.q_controller.f32IAccK_1 = 1;
    state.d_controller.f16InErrK_1 = 1;
    state.q_controller.f16InErrK_1 = 1;

    motor_foc_step(&state, &input, &output);

    assert(state.stop_d_integrator == 0U);
    assert(state.stop_q_integrator == 0U);
    assert(state.d_controller.f16UpperLim == 0);
    assert(state.d_controller.f16LowerLim == 0);
    assert(state.q_controller.f16UpperLim == 0);
    assert(state.q_controller.f16LowerLim == 0);
    assert(state.d_controller.f32IAccK_1 == 0);
    assert(state.q_controller.f32IAccK_1 == 0);
    assert(state.d_controller.f16InErrK_1 == 0);
    assert(state.q_controller.f16InErrK_1 == 0);
    assert(state.d_controller.bLimFlag == 1U);
    assert(state.q_controller.bLimFlag == 1U);
    assert(output.voltage.f16D == 0);
    assert(output.voltage.f16Q == 0);
    assert(output.duty.f16A == INT16_MAX);
    assert(output.duty.f16B == INT16_C(0x6893));
    assert(output.duty.f16C == 0);
    assert(output.sector == 1U);
}

int motor_test_foc(void) {
    test_zero_bus_runs_limited_control_cycle();
    return 0;
}
