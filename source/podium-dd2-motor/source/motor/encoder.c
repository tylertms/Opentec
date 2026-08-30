#include "motor/encoder.h"

enum {
    MOTOR_ENCODER_INDEX_DRIVE_CURRENT = 491,
    MOTOR_ENCODER_DIRECTION_TOLERANCE = 10,
    MOTOR_ENCODER_DIRECTION_RUNNING_STATUS = 0xaaaaU,
    MOTOR_ENCODER_DIRECTION_FAILED_STATUS = 0xbbbbU,
};

/**
 * @brief Extends one official FTM2 quadrature overflow into the signed revolution offset.
 *
 * The board-selected encoder modulus is added or subtracted according to the hardware direction.
 *
 * @param state Persistent encoder position state.
 * @param modulus Board-selected encoder modulus.
 * @param increasing True when the FTM overflow direction is increasing.
 */
void motor_encoder_overflow_apply(MotorEncoderState *state, int32_t modulus, bool increasing) {
    state->revolution_offset += increasing ? modulus : -modulus;
}

/**
 * @brief Publishes the official extended encoder position when no overflow is pending.
 *
 * A pending overflow defers publication so the interrupt can update the revolution offset first.
 *
 * @param state Persistent encoder position state.
 * @param overflow_pending True while FTM2 has an unhandled overflow.
 * @param counter Current sixteen-bit FTM2 quadrature count.
 * @param position_limit Exclusive positive and negative safety limit.
 * @return Pending, updated, or out-of-range result.
 */
MotorEncoderPositionResult motor_encoder_position_update(MotorEncoderState *state,
                                                         bool overflow_pending, uint16_t counter,
                                                         int32_t position_limit) {
    if (overflow_pending) {
        return kMotorEncoderPositionPending;
    }

    state->position = (int32_t)counter - state->zero_counter + state->revolution_offset;
    if (state->position >= position_limit || state->position <= -position_limit) {
        return kMotorEncoderPositionOutOfRange;
    }
    return kMotorEncoderPositionUpdated;
}

/**
 * @brief Clears the official extended encoder revolution and published position state.
 *
 * The captured index zero remains available while the live revolution and position return to zero.
 *
 * @param state Persistent encoder position state.
 */
void motor_encoder_position_reset(MotorEncoderState *state) {
    state->revolution_offset = 0;
    state->position = 0;
}

/**
 * @brief Resolves the official encoder position within one revolution.
 *
 * Counts before the captured zero wrap through the supplied hardware modulus.
 *
 * @param counter Current hardware quadrature counter.
 * @param zero_counter Captured encoder zero counter.
 * @param modulus Board-selected encoder counts per revolution.
 * @return Unsigned encoder position relative to zero with one-revolution wrapping.
 */
uint16_t motor_encoder_relative_position(uint16_t counter, uint16_t zero_counter,
                                         uint16_t modulus) {
    int32_t position = (int32_t)counter - zero_counter;
    if (position < 0) {
        position += modulus;
    }
    return (uint16_t)position;
}

/**
 * @brief Resolves one official encoder-index seek step.
 *
 * The fixed search current remains active until the index arrives or the countdown expires.
 *
 * @param index_detected True after the PORTE index interrupt captures a position.
 * @param timeout_remaining Active five-thousand-tick search countdown.
 * @return Drive current, countdown state, and completion result for the search.
 */
MotorEncoderIndexSeekStep motor_encoder_index_seek_step(bool index_detected,
                                                        uint16_t timeout_remaining) {
    if (!index_detected && timeout_remaining != 0U) {
        return (MotorEncoderIndexSeekStep){
            .drive_current = MOTOR_ENCODER_INDEX_DRIVE_CURRENT,
            .countdown_active = true,
        };
    }

    return (MotorEncoderIndexSeekStep){.complete = true};
}

/**
 * @brief Resets the official encoder-direction diagnostic sequence.
 *
 * A fresh diagnostic begins before the first forward index capture.
 *
 * @param state Diagnostic phase, captured positions, and reporting status.
 */
void motor_encoder_direction_initialize(MotorEncoderDirectionState *state) {
    *state = (MotorEncoderDirectionState){0};
}

/**
 * @brief Advances the official two-index encoder-direction diagnostic.
 *
 * Forward index captures establish one revolution before the motor returns to its start position.
 *
 * @param state Persistent diagnostic phase and captured positions.
 * @param index_seek_complete True when the active index search detected an index or timed out.
 * @param position Current extended encoder position.
 * @param encoder_modulus Expected encoder counts per revolution.
 * @return Diagnostic result and the drive, status, and index-search actions to apply.
 */
MotorEncoderDirectionStep motor_encoder_direction_check_step(MotorEncoderDirectionState *state,
                                                             bool index_seek_complete,
                                                             int32_t position,
                                                             int32_t encoder_modulus) {
    MotorEncoderDirectionStep step = {
        .status = state->status,
    };

    if (state->phase == kMotorEncoderDirectionBegin) {
        state->start_position = position;
        state->status = MOTOR_ENCODER_DIRECTION_RUNNING_STATUS;
        state->phase = kMotorEncoderDirectionFirstIndex;
        step.status = state->status;
        step.reset_controller = true;
        step.reset_position = true;
        step.restart_index_seek = true;
        return step;
    }

    if (state->phase == kMotorEncoderDirectionFirstIndex) {
        if (!index_seek_complete) {
            step.drive_current = MOTOR_ENCODER_INDEX_DRIVE_CURRENT;
            return step;
        }

        state->first_index_position = position;
        state->phase = kMotorEncoderDirectionSecondIndex;
        step.reset_position = true;
        step.restart_index_seek = true;
        return step;
    }

    if (state->phase == kMotorEncoderDirectionSecondIndex) {
        if (!index_seek_complete) {
            step.drive_current = MOTOR_ENCODER_INDEX_DRIVE_CURRENT;
            return step;
        }

        int32_t error = position - encoder_modulus - state->first_index_position;
        if (error < MOTOR_ENCODER_DIRECTION_TOLERANCE &&
            error > -MOTOR_ENCODER_DIRECTION_TOLERANCE) {
            state->phase = kMotorEncoderDirectionReturn;
            return step;
        }

        state->phase = kMotorEncoderDirectionBegin;
        state->status = MOTOR_ENCODER_DIRECTION_FAILED_STATUS;
        step.status = state->status;
        step.result = kMotorEncoderDirectionFailed;
        return step;
    }

    if (state->phase != kMotorEncoderDirectionReturn) {
        return step;
    }

    if (position > state->start_position) {
        step.drive_current = -MOTOR_ENCODER_INDEX_DRIVE_CURRENT;
        return step;
    }

    state->phase = kMotorEncoderDirectionBegin;
    state->status = 0U;
    step.status = state->status;
    step.result = kMotorEncoderDirectionPassed;
    return step;
}
