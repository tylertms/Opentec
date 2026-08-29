#include "motor/encoder_calibration.h"

#include <limits.h>
#include <string.h>

enum {
    MOTOR_ENCODER_CALIBRATION_SWEEP_VELOCITY = 327,
    MOTOR_ENCODER_CALIBRATION_CENTER_VELOCITY = 655,
    MOTOR_ENCODER_CALIBRATION_VELOCITY_TOLERANCE = 32,
    MOTOR_ENCODER_CALIBRATION_SETTLE_COUNT = 1000,
    MOTOR_ENCODER_CALIBRATION_CENTER_TOLERANCE = 100,
    MOTOR_ENCODER_CALIBRATION_SAMPLE_DIVISOR = 10,
    MOTOR_ENCODER_CALIBRATION_VERSION = 3U,
    MOTOR_ENCODER_CALIBRATION_SCALE = 0x3333U,
    MOTOR_ENCODER_CALIBRATION_SAMPLE_OFFSET = 2U,
    MOTOR_ENCODER_CORRECTION_DIRECTION_THRESHOLD = 82,
};

#define MOTOR_ENCODER_CALIBRATION_MAGIC UINT32_C(0xaaaaaaaa)

_Static_assert(sizeof(MotorEncoderCalibrationRecord) == 0x2558U,
               "unexpected encoder calibration record size");

/**
 * @brief Resolves the calibration velocity magnitude.
 *
 * The most-negative signed value retains its wrapped representation, matching the official
 * fixed-point absolute-value primitive.
 *
 * @param value Signed calibration velocity.
 * @return Nonnegative magnitude, or the retained most-negative value.
 */
static int16_t motor_encoder_calibration_abs(int16_t value) {
    if (value < 0 && value != INT16_MIN) {
        return (int16_t)-value;
    }
    return value;
}

/**
 * @brief Tests the active calibration velocity window.
 *
 * Both recovered limits are inclusive.
 *
 * @param state Calibration state containing the active velocity limits.
 * @param velocity Signed velocity sample to test.
 * @return True when the sample is inside the active window.
 */
static bool motor_encoder_calibration_velocity_ready(const MotorEncoderCalibrationState *state,
                                                     int16_t velocity) {
    return velocity >= state->velocity_lower && velocity <= state->velocity_upper;
}

/**
 * @brief Captures one eligible encoder correction sample.
 *
 * Every tenth relative encoder position advances the shift-three correction filter and stores the
 * result in the selected directional table.
 *
 * @param state Calibration filter and persistent correction tables.
 * @param reverse True to update the reverse table, or false to update the forward table.
 * @param relative_position Encoder position within one revolution.
 * @param correction Signed velocity-controller error sample.
 */
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
 * @brief Initializes the seven-phase encoder correction capture sequence.
 *
 * Calibration begins with a zero velocity target and a shift-three correction filter while the
 * persistent record is cleared for a fresh pair of directional sweeps.
 *
 * @param state Calibration phase, filter, captured table, and persistent record.
 */
void motor_encoder_calibration_initialize(MotorEncoderCalibrationState *state) {
    memset(state, 0, sizeof(*state));
    state->correction_filter.shift = 3U;
}

/**
 * @brief Advances one encoder correction calibration sample.
 *
 * The state machine settles at each recovered velocity target, captures one complete revolution
 * in each direction, returns the shaft to center, and finalizes the persistent record header.
 *
 * @param state Persistent calibration phase, velocity window, filter, and correction tables.
 * @param input Current velocity, correction error, position, encoder phase, and revolution event.
 * @return Velocity and synchronization actions plus the completion result.
 */
MotorEncoderCalibrationStep
motor_encoder_calibration_step(MotorEncoderCalibrationState *state,
                               const MotorEncoderCalibrationInput *input) {
    MotorEncoderCalibrationStep step = {
        .target_velocity = state->target_velocity,
    };

    switch (state->phase) {
    case kMotorEncoderCalibrationInitialize:
        state->target_velocity = 0;
        state->phase = kMotorEncoderCalibrationStartForward;
        step.target_velocity = state->target_velocity;
        step.reset_controller = true;
        return step;
    case kMotorEncoderCalibrationStartForward:
        state->target_velocity = MOTOR_ENCODER_CALIBRATION_SWEEP_VELOCITY;
        state->velocity_lower =
            MOTOR_ENCODER_CALIBRATION_SWEEP_VELOCITY - MOTOR_ENCODER_CALIBRATION_VELOCITY_TOLERANCE;
        state->velocity_upper =
            MOTOR_ENCODER_CALIBRATION_SWEEP_VELOCITY + MOTOR_ENCODER_CALIBRATION_VELOCITY_TOLERANCE;
        state->phase = kMotorEncoderCalibrationSettleForward;
        step.target_velocity = state->target_velocity;
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
        state->target_velocity = -MOTOR_ENCODER_CALIBRATION_SWEEP_VELOCITY;
        state->phase = kMotorEncoderCalibrationSettleReverse;
        step.target_velocity = state->target_velocity;
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
        state->target_velocity = input->position > 0 ? -MOTOR_ENCODER_CALIBRATION_CENTER_VELOCITY
                                                     : MOTOR_ENCODER_CALIBRATION_CENTER_VELOCITY;
        state->phase = kMotorEncoderCalibrationCenter;
        step.target_velocity = state->target_velocity;
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
        state->target_velocity = 0;
        state->phase = kMotorEncoderCalibrationInitialize;
        step.target_velocity = state->target_velocity;
        step.result = kMotorEncoderCalibrationComplete;
        return step;
    }

    return step;
}

/**
 * @brief Reads and scales one official directional encoder correction sample.
 *
 * Direction applies the persisted sample offset before the board-selected table wrap and Q15 scale.
 *
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

/**
 * @brief Updates the encoder correction-table direction.
 *
 * The filtered position delta selects a new table only after crossing the official
 * direction threshold. Motion inside the dead band retains the previous selection.
 *
 * @param reverse Previously selected correction-table direction.
 * @param filtered_position_delta Filtered position change for the current control cycle.
 * @return True when the reverse correction table must be used.
 */
bool motor_encoder_correction_direction_update(bool reverse, int16_t filtered_position_delta) {
    if (filtered_position_delta >= MOTOR_ENCODER_CORRECTION_DIRECTION_THRESHOLD) {
        return false;
    }
    if (filtered_position_delta <= -MOTOR_ENCODER_CORRECTION_DIRECTION_THRESHOLD) {
        return true;
    }
    return reverse;
}

/**
 * @brief Validates the official persisted encoder correction record header.
 *
 * Both the fixed magic and recovered calibration version must match.
 *
 * @param record Encoder correction record loaded from calibration flash.
 * @return True when the magic and version match the official format.
 */
bool motor_encoder_calibration_record_is_valid(const MotorEncoderCalibrationRecord *record) {
    return record->magic == MOTOR_ENCODER_CALIBRATION_MAGIC &&
           record->version == MOTOR_ENCODER_CALIBRATION_VERSION;
}
