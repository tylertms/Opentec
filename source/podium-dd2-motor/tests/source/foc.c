#include "motor/foc.h"

#include <assert.h>

static void test_zero_bus_publishes_limiter_state(void) {
    MotorFocState state = {0};
    MotorFocInput input = {.dc_bus_voltage = 0};
    MotorFocOutput output = {0};

    motor_foc_step(&state, &input, &output);

    assert(state.stop_d_integrator == 1U);
    assert(state.stop_q_integrator == 1U);
    assert(state.d_controller.bLimFlag == 1U);
    assert(state.q_controller.bLimFlag == 1U);
    assert(output.voltage.f16D == 0);
    assert(output.voltage.f16Q == 0);
    assert(output.duty.f16A == 0x4000);
    assert(output.duty.f16B == 0x4000);
    assert(output.duty.f16C == 0x4000);
    assert(output.sector == 0U);
}

int motor_test_foc(void) {
    test_zero_bus_publishes_limiter_state();
    return 0;
}
