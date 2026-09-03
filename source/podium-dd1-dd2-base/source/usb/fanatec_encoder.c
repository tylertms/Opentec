#include "usb/fanatec_encoder.h"

#include <stdbool.h>
#include <stdint.h>

#include "platform/time.h"

/** @brief Fanatec encoder timing and report-field constants. */
enum {
    ENCODER_PHASE_DURATION_MS =
        17, /**< Duration of each direction or quiet phase in milliseconds. */
    ENCODER_DIRECTION_BUTTON_BANK = 3, /**< Button bank containing encoder direction bits. */
    ENCODER_NEGATIVE_BUTTON = 1 << 2,  /**< Encoder negative-direction button bit. */
    ENCODER_POSITIVE_BUTTON = 1 << 3,  /**< Encoder positive-direction button bit. */
};

/**
 * @brief Writes the current encoder direction buttons.
 *
 * Clears both direction bits, then sets the negative or positive bit for a nonzero direction.
 *
 * @param[in,out] buttons Fanatec direction-byte field to update.
 * @param[in] direction Negative one, zero, or positive one.
 */
static void apply_direction(uint8_t *buttons, int8_t direction) {
    *buttons &= (uint8_t)~(ENCODER_NEGATIVE_BUTTON | ENCODER_POSITIVE_BUTTON);
    if (direction < 0) {
        *buttons |= ENCODER_NEGATIVE_BUTTON;
    } else if (direction > 0) {
        *buttons |= ENCODER_POSITIVE_BUTTON;
    }
}

void fanatec_encoder_init(FanatecEncoder *encoder) { *encoder = (FanatecEncoder){0}; }

/**
 * @brief Advances one Fanatec encoder channel.
 *
 * Presents the current direction, consumes a pending step only strictly after its deadline, and
 * inserts the official quiet phase before another step can be consumed.
 *
 * @param[in,out] encoder Encoder position state.
 * @param[in] pending_direction Requested direction.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in,out] deadline_ms Channel phase deadline.
 * @param[in,out] quiet_phase Whether the channel is in its quiet phase.
 * @param[out] buttons Channel direction bits.
 * @param[in,out] input Fanatec input receiving the position.
 * @return True when a pending step was consumed.
 */
static bool update_channel(FanatecEncoder *encoder, int8_t pending_direction, uint32_t now_ms,
                           uint32_t *deadline_ms, bool *quiet_phase, uint8_t *buttons,
                           fanatec_input_state *input) {
    int8_t direction = pending_direction < 0 ? -1 : pending_direction > 0 ? 1 : 0;
    input->encoder_position = (int8_t)encoder->position;

    if (*quiet_phase) {
        apply_direction(buttons, 0);
        if (platform_time_reached(now_ms, *deadline_ms + 1u)) {
            *quiet_phase = false;
            if (direction != 0) {
                *deadline_ms = now_ms + ENCODER_PHASE_DURATION_MS;
            }
        }
        return false;
    }

    apply_direction(buttons, direction);
    if (direction == 0) {
        *deadline_ms = now_ms + ENCODER_PHASE_DURATION_MS;
        return false;
    }
    if (!platform_time_reached(now_ms, *deadline_ms + 1u)) {
        return false;
    }

    encoder->position = (uint8_t)(encoder->position + direction);
    *deadline_ms = now_ms + ENCODER_PHASE_DURATION_MS;
    *quiet_phase = true;
    input->encoder_position = (int8_t)encoder->position;
    return true;
}

bool fanatec_encoder_update(FanatecEncoder *encoder, int8_t pending_direction, uint32_t now_ms,
                            fanatec_input_state *input) {
    return update_channel(encoder, pending_direction, now_ms, &encoder->deadline_ms,
                          &encoder->quiet_phase,
                          &input->button_banks[ENCODER_DIRECTION_BUTTON_BANK], input);
}

/**
 * @brief Advances the secondary encoder direction presentation.
 *
 * @param[in,out] encoder Encoder presentation state.
 * @param[in] pending_direction Sign of pending secondary motion.
 * @param[in] now_ms Current monotonic time.
 * @param[in,out] input Fanatec input receiving direction state.
 * @return True when one pending step must be consumed; otherwise false.
 */
bool fanatec_encoder_update_secondary(FanatecEncoder *encoder, int8_t pending_direction,
                                      uint32_t now_ms, fanatec_input_state *input) {
    return update_channel(encoder, pending_direction, now_ms, &encoder->secondary_deadline_ms,
                          &encoder->secondary_quiet_phase, &input->rotary[3], input);
}
