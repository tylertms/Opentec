#include <assert.h>

#include "motor/calibration.h"

static void submit_samples(MotorCurrentCalibrationState *state, uint16_t sample) {
    for (uint32_t index = 0U; index < MOTOR_CURRENT_CALIBRATION_SAMPLE_COUNT; ++index) {
        assert(motor_current_calibration_step(state, true, sample) ==
               kMotorCurrentCalibrationPending);
    }
}

int motor_test_current_calibration(void) {
    MotorCurrentCalibrationState state = {
        .offsets = {.phase_a = 111, .phase_b = 222},
    };

    motor_current_calibration_start(&state);
    assert(state.stage == kMotorCurrentCalibrationPhaseA);
    assert(state.offsets.phase_a == 111);
    assert(state.offsets.phase_b == 222);

    submit_samples(&state, 1234U);
    assert(state.offsets.phase_a == 111);
    assert(motor_current_calibration_step(&state, false, 0U) ==
           kMotorCurrentCalibrationPhaseBStarted);
    assert(state.offsets.phase_a == 1234);
    assert(state.stage == kMotorCurrentCalibrationPhaseB);

    submit_samples(&state, 2345U);
    assert(motor_current_calibration_step(&state, false, 0U) == kMotorCurrentCalibrationFinished);
    assert(state.offsets.phase_b == 2345);
    assert(state.stage == kMotorCurrentCalibrationComplete);
    assert(state.sample_count == 0U);
    assert(state.sample_sum == 0U);

    assert(motor_current_calibration_step(&state, true, 500U) == kMotorCurrentCalibrationPending);
    assert(state.offsets.phase_b == 2345);

    return 0;
}
