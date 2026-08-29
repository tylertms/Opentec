#include "common/motor/encoder.h"

/**
 * @brief Extends one official FTM2 quadrature overflow into the signed revolution offset.
 * @param state Persistent encoder position state.
 * @param modulus Board-selected encoder modulus.
 * @param increasing True when the FTM overflow direction is increasing.
 */
void motor_encoder_overflow_apply(MotorEncoderState *state, int32_t modulus, bool increasing) {
    state->revolution_offset += increasing ? modulus : -modulus;
}

/**
 * @brief Publishes the official extended encoder position when no overflow is pending.
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
 * @param state Persistent encoder position state.
 */
void motor_encoder_position_reset(MotorEncoderState *state) {
    state->revolution_offset = 0;
    state->position = 0;
}
