#include "motor/center.h"

enum {
    CENTERED_POSITION_LIMIT = 0x143c0,
};

/**
 * @brief Applies an official center command and normalizes the circular encoder offset.
 *
 * A changed command retains the supplied circular offset inside one encoder revolution and
 * normalizes either shared endpoint according to the raw timer counter.
 *
 * @param state Persistent requested center and activation state.
 * @param requested New signed center command from the motor link.
 * @param encoder_modulus Positive encoder offset limit for one revolution.
 * @param encoder_counter Current raw encoder timer count.
 * @param wrap_threshold Timer count separating the two representations of the shared endpoint.
 * @param encoder_offset Persistent signed revolution offset to normalize.
 * @return True when an active center command changed.
 */
bool motor_center_command_apply(MotorCenterState *state, int16_t requested, int32_t encoder_modulus,
                                uint16_t encoder_counter, uint16_t wrap_threshold,
                                int32_t *encoder_offset) {
    if (!state->active || state->requested == requested) {
        return false;
    }

    state->requested = requested;
    if (*encoder_offset > encoder_modulus) {
        *encoder_offset = encoder_modulus;
    } else if (*encoder_offset < -encoder_modulus) {
        *encoder_offset = -encoder_modulus;
    } else if (*encoder_offset == -encoder_modulus && encoder_counter < wrap_threshold) {
        *encoder_offset = 0;
    } else if (*encoder_offset == encoder_modulus && encoder_counter >= wrap_threshold) {
        *encoder_offset = 0;
    }
    return true;
}

/**
 * @brief Resolves the official centered force-feedback position.
 *
 * The commanded center is removed from the extended encoder position before the result is limited
 * to the range consumed by the force-feedback engine.
 *
 * @param position Current extended encoder position.
 * @param center Signed center command from the motor link.
 * @return Centered position clamped to plus or minus 82,880 counts.
 */
int32_t motor_centered_position_resolve(int32_t position, int16_t center) {
    int32_t centered = position - center;
    if (centered > CENTERED_POSITION_LIMIT) {
        return CENTERED_POSITION_LIMIT;
    }
    if (centered < -CENTERED_POSITION_LIMIT) {
        return -CENTERED_POSITION_LIMIT;
    }
    return centered;
}
