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
 * @param[in,out] input Fanatec input state to update.
 * @param[in] direction Negative one, zero, or positive one.
 */
static void apply_direction_buttons(fanatec_input_state *input, int8_t direction) {
    uint8_t buttons = input->button_banks[ENCODER_DIRECTION_BUTTON_BANK] &
                      (uint8_t)~(ENCODER_NEGATIVE_BUTTON | ENCODER_POSITIVE_BUTTON);
    if (direction < 0) {
        buttons |= ENCODER_NEGATIVE_BUTTON;
    } else if (direction > 0) {
        buttons |= ENCODER_POSITIVE_BUTTON;
    }
    input->button_banks[ENCODER_DIRECTION_BUTTON_BANK] = buttons;
}

void fanatec_encoder_init(FanatecEncoder *encoder) { *encoder = (FanatecEncoder){0}; }

bool fanatec_encoder_update(FanatecEncoder *encoder, int8_t pending_direction, uint32_t now_ms,
                            fanatec_input_state *input) {
    int8_t direction = pending_direction < 0 ? -1 : pending_direction > 0 ? 1 : 0;
    input->encoder_position = (int8_t)encoder->position;

    if (encoder->quiet_phase) {
        apply_direction_buttons(input, 0);
        if (platform_time_reached(now_ms, encoder->deadline_ms)) {
            encoder->quiet_phase = false;
            if (direction != 0) {
                encoder->deadline_ms = now_ms + ENCODER_PHASE_DURATION_MS;
            }
        }
        return false;
    }

    apply_direction_buttons(input, direction);
    if (direction == 0) {
        encoder->deadline_ms = now_ms + ENCODER_PHASE_DURATION_MS;
        return false;
    }
    if (!platform_time_reached(now_ms, encoder->deadline_ms)) {
        return false;
    }

    encoder->position = (uint8_t)(encoder->position + direction);
    encoder->deadline_ms = now_ms + ENCODER_PHASE_DURATION_MS;
    encoder->quiet_phase = true;
    input->encoder_position = (int8_t)encoder->position;
    return true;
}
