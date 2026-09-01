#include "motor/calibration.h"

/**
 * @brief Starts the two-phase current-offset calibration sequence.
 *
 * Existing offsets remain available until both new phase averages have completed.
 *
 * @param[in,out] state Calibration state while preserving the last valid offsets.
 */
void motor_current_calibration_start(MotorCurrentCalibrationState *state) {
    state->stage = kMotorCurrentCalibrationPhaseA;
    state->sample_count = 0U;
    state->sample_sum = 0U;
}

/**
 * @brief Accumulates 1024 samples for each phase and publishes their averages.
 *
 * Phase A completes before phase B begins, and only ready conversions advance either average.
 *
 * @param[in,out] state Current calibration stage, accumulation, and resulting offsets.
 * @param[in] sample_ready True when the active ADC conversion completed.
 * @param[in] sample Raw unsigned ADC conversion result.
 * @return Pending, phase-B transition, or completed calibration.
 */
MotorCurrentCalibrationResult motor_current_calibration_step(MotorCurrentCalibrationState *state,
                                                             bool sample_ready, uint16_t sample) {
    if (state->stage != kMotorCurrentCalibrationPhaseA &&
        state->stage != kMotorCurrentCalibrationPhaseB) {
        return kMotorCurrentCalibrationPending;
    }

    if (state->sample_count >= MOTOR_CURRENT_CALIBRATION_SAMPLE_COUNT) {
        int16_t average = (int16_t)(state->sample_sum >> 10U);
        MotorCurrentCalibrationResult result;

        if (state->stage == kMotorCurrentCalibrationPhaseA) {
            state->offsets.phase_a = average;
            state->stage = kMotorCurrentCalibrationPhaseB;
            result = kMotorCurrentCalibrationPhaseBStarted;
        } else {
            state->offsets.phase_b = average;
            state->stage = kMotorCurrentCalibrationComplete;
            result = kMotorCurrentCalibrationFinished;
        }

        state->sample_count = 0U;
        state->sample_sum = 0U;
        return result;
    }

    if (sample_ready) {
        ++state->sample_count;
        state->sample_sum += sample;
    }

    return kMotorCurrentCalibrationPending;
}
