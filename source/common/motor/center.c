#include "common/motor/center.h"

/**
 * @brief Applies an official center command and normalizes the circular encoder offset.
 * @param state Persistent requested center, offset, and activation state.
 * @param requested New signed center command from the motor link.
 * @param encoder_modulus Positive encoder offset limit for one revolution.
 * @param encoder_counter Current raw encoder timer count.
 * @param wrap_threshold Timer count separating the two representations of the shared endpoint.
 * @return True when an active center command changed.
 */
bool motor_center_command_apply(MotorCenterState *state, int16_t requested, int32_t encoder_modulus,
                                uint16_t encoder_counter, uint16_t wrap_threshold) {
    if (!state->active || state->requested == requested) {
        return false;
    }

    state->requested = requested;
    if (state->encoder_offset > encoder_modulus) {
        state->encoder_offset = encoder_modulus;
    } else if (state->encoder_offset < -encoder_modulus) {
        state->encoder_offset = -encoder_modulus;
    } else if (state->encoder_offset == -encoder_modulus && encoder_counter < wrap_threshold) {
        state->encoder_offset = 0;
    } else if (state->encoder_offset == encoder_modulus && encoder_counter >= wrap_threshold) {
        state->encoder_offset = 0;
    }
    return true;
}
