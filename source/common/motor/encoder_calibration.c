#include "common/motor/encoder_calibration.h"

#include <limits.h>

enum {
    MOTOR_ENCODER_CALIBRATION_DRIVE_CURRENT = 327,
    MOTOR_ENCODER_CALIBRATION_CENTER_CURRENT = 655,
    MOTOR_ENCODER_CALIBRATION_VELOCITY_TOLERANCE = 32,
    MOTOR_ENCODER_CALIBRATION_SETTLE_COUNT = 1000,
    MOTOR_ENCODER_CALIBRATION_CENTER_TOLERANCE = 100,
    MOTOR_ENCODER_CALIBRATION_SAMPLE_DIVISOR = 10,
    MOTOR_ENCODER_CALIBRATION_VERSION = 3U,
    MOTOR_ENCODER_CALIBRATION_SCALE = 0x3333U,
    MOTOR_ENCODER_CALIBRATION_SAMPLE_OFFSET = 2U,
};

#define MOTOR_ENCODER_CALIBRATION_MAGIC UINT32_C(0xaaaaaaaa)

_Static_assert(sizeof(MotorEncoderCalibrationRecord) == 0x2558U,
               "unexpected encoder calibration record size");

static int16_t motor_encoder_calibration_abs(int16_t value) {
    if (value < 0 && value != INT16_MIN) {
        return (int16_t)-value;
    }
    return value;
}

static bool motor_encoder_calibration_velocity_ready(const MotorEncoderCalibrationState *state,
                                                     int16_t velocity) {
    return velocity >= state->velocity_lower && velocity <= state->velocity_upper;
}

static void motor_encoder_calibration_sample(MotorEncoderCalibrationState *state, bool reverse,
                                             uint16_t relative_position, int16_t correction) {
    if (relative_position % MOTOR_ENCODER_CALIBRATION_SAMPLE_DIVISOR != 0U) {
        return;
    }

    uint16_t index = relative_position / MOTOR_ENCODER_CALIBRATION_SAMPLE_DIVISOR;
    if (index >= MOTOR_ENCODER_CORRECTION_CAPACITY) {
        return;
    }

    int16_t filtered = motor_motion_filter_step(&state->correction_filter, correction);
    if (reverse) {
        state->record.reverse[index] = filtered;
    } else {
        state->record.forward[index] = filtered;
    }
}

/**
 * @brief Initializes the official seven-phase encoder correction capture sequence.
 * @param state Calibration phase, filter, captured table, and persistent record.
 */
void motor_encoder_calibration_initialize(MotorEncoderCalibrationState *state) {
    *state = (MotorEncoderCalibrationState){
        .correction_filter.shift = 3U,
    };
}

/**
 * @brief Advances one official encoder correction calibration sample.
 * @param state Persistent calibration phase, velocity window, filter, and correction tables.
 * @param input Current velocity, correction error, position, encoder phase, and revolution event.
 * @return Drive and synchronization actions plus the completion result.
 */
MotorEncoderCalibrationStep
motor_encoder_calibration_step(MotorEncoderCalibrationState *state,
                               const MotorEncoderCalibrationInput *input) {
    MotorEncoderCalibrationStep step = {
        .drive_current = state->drive_current,
    };

    switch (state->phase) {
    case kMotorEncoderCalibrationInitialize:
        state->drive_current = 0;
        state->phase = kMotorEncoderCalibrationStartForward;
        step.drive_current = state->drive_current;
        step.reset_controller = true;
        return step;
    case kMotorEncoderCalibrationStartForward:
        state->drive_current = MOTOR_ENCODER_CALIBRATION_DRIVE_CURRENT;
        state->velocity_lower =
            MOTOR_ENCODER_CALIBRATION_DRIVE_CURRENT - MOTOR_ENCODER_CALIBRATION_VELOCITY_TOLERANCE;
        state->velocity_upper =
            MOTOR_ENCODER_CALIBRATION_DRIVE_CURRENT + MOTOR_ENCODER_CALIBRATION_VELOCITY_TOLERANCE;
        state->phase = kMotorEncoderCalibrationSettleForward;
        step.drive_current = state->drive_current;
        return step;
    case kMotorEncoderCalibrationSettleForward:
        ++state->settle_count;
        if (state->settle_count < MOTOR_ENCODER_CALIBRATION_SETTLE_COUNT ||
            !motor_encoder_calibration_velocity_ready(state, input->velocity) ||
            input->position < (int32_t)(input->encoder_period / 2U)) {
            return step;
        }
        state->settle_count = 0U;
        state->sweep_start_position = input->relative_position;
        state->phase = kMotorEncoderCalibrationCaptureForward;
        step.arm_revolution = true;
        return step;
    case kMotorEncoderCalibrationCaptureForward:
        motor_encoder_calibration_sample(state, false, input->relative_position, input->correction);
        if (!input->revolution_complete ||
            input->relative_position != state->sweep_start_position) {
            return step;
        }
        state->drive_current = -MOTOR_ENCODER_CALIBRATION_DRIVE_CURRENT;
        state->phase = kMotorEncoderCalibrationSettleReverse;
        step.drive_current = state->drive_current;
        step.clear_revolution = true;
        return step;
    case kMotorEncoderCalibrationSettleReverse:
        ++state->settle_count;
        if (state->settle_count < MOTOR_ENCODER_CALIBRATION_SETTLE_COUNT ||
            !motor_encoder_calibration_velocity_ready(
                state, motor_encoder_calibration_abs(input->velocity))) {
            return step;
        }
        state->settle_count = 0U;
        state->sweep_start_position = input->relative_position;
        state->phase = kMotorEncoderCalibrationCaptureReverse;
        step.arm_revolution = true;
        return step;
    case kMotorEncoderCalibrationCaptureReverse:
        motor_encoder_calibration_sample(state, true, input->relative_position, input->correction);
        if (!input->revolution_complete ||
            input->relative_position != state->sweep_start_position) {
            return step;
        }
        state->drive_current = input->position > 0 ? -MOTOR_ENCODER_CALIBRATION_CENTER_CURRENT
                                                   : MOTOR_ENCODER_CALIBRATION_CENTER_CURRENT;
        state->phase = kMotorEncoderCalibrationCenter;
        step.drive_current = state->drive_current;
        step.clear_revolution = true;
        return step;
    case kMotorEncoderCalibrationCenter:
        if (input->position < -MOTOR_ENCODER_CALIBRATION_CENTER_TOLERANCE ||
            input->position > MOTOR_ENCODER_CALIBRATION_CENTER_TOLERANCE) {
            return step;
        }
        state->record.magic = MOTOR_ENCODER_CALIBRATION_MAGIC;
        state->record.version = MOTOR_ENCODER_CALIBRATION_VERSION;
        state->record.correction_scale = MOTOR_ENCODER_CALIBRATION_SCALE;
        state->record.sample_offset = MOTOR_ENCODER_CALIBRATION_SAMPLE_OFFSET;
        state->drive_current = 0;
        state->phase = kMotorEncoderCalibrationInitialize;
        step.drive_current = state->drive_current;
        step.result = kMotorEncoderCalibrationComplete;
        return step;
    }

    return step;
}

/**
 * @brief Reads and scales one official directional encoder correction sample.
 * @param record Valid persisted encoder correction record.
 * @param reverse True for the reverse-direction correction table.
 * @param relative_position Encoder position within one revolution.
 * @param table_length Board-selected correction-table wrap length.
 * @return Signed scaled encoder correction.
 */
int16_t motor_encoder_correction_read(const MotorEncoderCalibrationRecord *record, bool reverse,
                                      uint16_t relative_position, uint16_t table_length) {
    int32_t index = relative_position / MOTOR_ENCODER_CALIBRATION_SAMPLE_DIVISOR;
    index += reverse ? -(int32_t)record->sample_offset : (int32_t)record->sample_offset;
    if (index < 0) {
        index += (int32_t)table_length - 1;
    } else if (index >= table_length) {
        index -= (int32_t)table_length - 1;
    }

    int16_t correction = reverse ? record->reverse[index] : record->forward[index];
    return motor_q15_scale_saturate(record->correction_scale, correction);
}
