#include "motor/pi.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

static GFLIB_CTRL_PI_P_AW_T_A32 controller(void) {
    return (GFLIB_CTRL_PI_P_AW_T_A32){
        .a32PGain = 0x9999,
        .a32IGain = 0x147,
        .f16UpperLim = 0x1999,
        .f16LowerLim = -6553,
    };
}

static int32_t reference_floor_q16(int64_t value) {
    int64_t result = value / INT64_C(65536);
    if (value < 0 && value % INT64_C(65536) != 0) {
        --result;
    }
    return (int32_t)result;
}

static int32_t reference_ceil_q16(int64_t value) {
    int64_t result = value / INT64_C(65536);
    if (value > 0 && value % INT64_C(65536) != 0) {
        ++result;
    }
    return (int32_t)result;
}

static frac16_t reference_step(frac16_t error, const bool_t *stop_integrator,
                               GFLIB_CTRL_PI_P_AW_T_A32 *state) {
    int32_t integration_error = state->f16InErrK_1;
    if (*stop_integrator == 0U) {
        integration_error += error;
    }
    int64_t integral = (int64_t)integration_error * state->a32IGain + state->f32IAccK_1;
    int32_t integral_value = reference_ceil_q16(integral);
    if (integral_value > state->f16UpperLim) {
        state->f32IAccK_1 = (int32_t)state->f16UpperLim * INT32_C(65536);
    } else if (integral_value < state->f16LowerLim) {
        state->f32IAccK_1 = (int32_t)state->f16LowerLim * INT32_C(65536);
    } else {
        state->f32IAccK_1 = (int32_t)integral;
    }
    state->f16InErrK_1 = error;

    int64_t output_accumulator = (int64_t)error * state->a32PGain * INT64_C(2) + state->f32IAccK_1;
    int32_t output = reference_floor_q16(output_accumulator);
    state->bLimFlag = 0U;
    if (output >= state->f16UpperLim) {
        output = state->f16UpperLim;
        state->bLimFlag = 1U;
    }
    if (output <= state->f16LowerLim) {
        output = state->f16LowerLim;
        state->bLimFlag = 1U;
    }
    return (frac16_t)output;
}

static void assert_state_equal(const GFLIB_CTRL_PI_P_AW_T_A32 *actual,
                               const GFLIB_CTRL_PI_P_AW_T_A32 *expected) {
    assert(actual->a32PGain == expected->a32PGain);
    assert(actual->a32IGain == expected->a32IGain);
    assert(actual->f32IAccK_1 == expected->f32IAccK_1);
    assert(actual->f16InErrK_1 == expected->f16InErrK_1);
    assert(actual->f16UpperLim == expected->f16UpperLim);
    assert(actual->f16LowerLim == expected->f16LowerLim);
    assert(actual->bLimFlag == expected->bLimFlag);
}

static void test_reference_rounding(void) {
    GFLIB_CTRL_PI_P_AW_T_A32 state = controller();
    bool_t stop = 0U;
    assert(motor_pi_step(1, &stop, &state) == 1);
    assert(state.f32IAccK_1 == 0x147);
    assert(state.f16InErrK_1 == 1);

    state = controller();
    assert(motor_pi_step(1000, &stop, &state) == 1204);
}

static void test_stopped_error_state(void) {
    GFLIB_CTRL_PI_P_AW_T_A32 state = controller();
    bool_t stop = 0U;
    assert(motor_pi_step(1, &stop, &state) == 1);
    stop = 1U;
    assert(motor_pi_step(2, &stop, &state) == 2);
    assert(state.f32IAccK_1 == 0x28e);
    assert(state.f16InErrK_1 == 2);
}

static void test_limits(void) {
    GFLIB_CTRL_PI_P_AW_T_A32 state = {
        .a32PGain = 0,
        .a32IGain = 0x10000,
        .f16UpperLim = 1,
        .f16LowerLim = -1,
    };
    bool_t stop = 0U;
    assert(motor_pi_step(1, &stop, &state) == 1);
    assert(state.f32IAccK_1 == 0x10000);
    assert(state.bLimFlag == 1U);
    assert(motor_pi_step(1, &stop, &state) == 1);
    assert(state.f32IAccK_1 == 0x10000);
}

static void test_exhaustive_reference_equivalence(void) {
    GFLIB_CTRL_PI_P_AW_T_A32 initial_states[] = {
        controller(),
        {
            .a32PGain = 0x18000,
            .a32IGain = 0x9000,
            .f32IAccK_1 = -0x12345678,
            .f16InErrK_1 = -12345,
            .f16UpperLim = 30000,
            .f16LowerLim = -30000,
        },
        {
            .a32PGain = 0x7fffffff,
            .a32IGain = 0x10001,
            .f32IAccK_1 = 0x23456789,
            .f16InErrK_1 = 32767,
            .f16UpperLim = 32767,
            .f16LowerLim = -32768,
        },
    };

    for (size_t state_index = 0; state_index < sizeof(initial_states) / sizeof(initial_states[0]);
         ++state_index) {
        for (uint32_t stop_value = 0; stop_value <= 1U; ++stop_value) {
            bool_t stop = (bool_t)stop_value;
            for (int32_t error_value = INT16_MIN; error_value <= INT16_MAX; ++error_value) {
                GFLIB_CTRL_PI_P_AW_T_A32 actual = initial_states[state_index];
                GFLIB_CTRL_PI_P_AW_T_A32 expected = initial_states[state_index];
                frac16_t error = (frac16_t)error_value;
                frac16_t actual_output = motor_pi_step(error, &stop, &actual);
                frac16_t expected_output = reference_step(error, &stop, &expected);
                assert(actual_output == expected_output);
                assert_state_equal(&actual, &expected);
            }
        }
    }
}

int motor_test_pi(void) {
    test_reference_rounding();
    test_stopped_error_state();
    test_limits();
    test_exhaustive_reference_equivalence();
    return 0;
}
