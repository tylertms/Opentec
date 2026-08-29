#include "usb/xbox_gip_input.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum {
    WHEEL_MODE_CRC = 6,
    WHEEL_MODE_NINE = 9,
    WHEEL_MODE_TEN = 10,
    WHEEL_MODE_ELEVEN = 11,
    WHEEL_MODE_CRC_AUTHENTICATED = 21,
    WHEEL_MODE_EXTENDED_BUTTON = 29,
};

/**
 * @brief Reads one bit from an input value.
 *
 * Extracts the requested bit as zero or one.
 *
 * @param[in] value Source value.
 * @param[in] bit Zero-based source bit.
 * @return Extracted bit value.
 */
static uint8_t read_bit(uint16_t value, uint8_t bit) { return (uint8_t)((value >> bit) & 1u); }

/**
 * @brief Maps normalized wheel buttons into the two primary GIP bytes.
 *
 * Reorders the primary wheel word into the Xbox direction, face, shoulder, and system positions,
 * adds the alternating packet bit, and merges the mode-specific extended system inputs.
 *
 * @param[in,out] builder Input builder containing the alternating packet state.
 * @param[in] state Current normalized wheel input.
 * @param[out] snapshot Xbox snapshot receiving the primary button bytes.
 */
static void map_primary_buttons(UsbXboxGipInputBuilder *builder, const UsbXboxGipInputState *state,
                                UsbXboxGipInputSnapshot *snapshot) {
    uint16_t primary = (uint16_t)state->buttons[0] | (uint16_t)state->buttons[1] << 8;
    uint16_t extended = (uint16_t)state->buttons[2] | (uint16_t)state->mode_buttons << 8;
    builder->alternate_packet_bit = !builder->alternate_packet_bit;
    snapshot->buttons[0] = (uint8_t)(read_bit(primary, 15) << 2 | read_bit(primary, 14) << 3 |
                                     read_bit(primary, 4) << 4 | read_bit(primary, 6) << 5 |
                                     read_bit(primary, 5) << 6 | read_bit(primary, 7) << 7 |
                                     (builder->alternate_packet_bit ? 2u : 0u));
    snapshot->buttons[1] = (uint8_t)(read_bit(primary, 0) | read_bit(primary, 3) << 1 |
                                     read_bit(primary, 1) << 2 | read_bit(primary, 2) << 3 |
                                     read_bit(primary, 11) << 4 | read_bit(primary, 8) << 5);
    if (state->wheel_mode != WHEEL_MODE_EXTENDED_BUTTON) {
        snapshot->buttons[0] |= (uint8_t)(read_bit(extended, 10) << 4);
    }
    if (state->wheel_mode == WHEEL_MODE_TEN || state->wheel_mode == 18) {
        snapshot->buttons[0] |= (uint8_t)(read_bit(extended, 9) << 4);
    }
}

/**
 * @brief Maps secondary wheel controls into GIP extension fields.
 *
 * Packs the four common secondary inputs, mode-specific control bits, six-button status field,
 * and the dedicated mode-29 extension input.
 *
 * @param[in] state Current normalized wheel input.
 * @param[out] snapshot Xbox snapshot receiving extension fields.
 */
static void map_extended_buttons(const UsbXboxGipInputState *state,
                                 UsbXboxGipInputSnapshot *snapshot) {
    uint16_t primary = (uint16_t)state->buttons[0] | (uint16_t)state->buttons[1] << 8;
    uint16_t extended = (uint16_t)state->buttons[2] | (uint16_t)state->mode_buttons << 8;
    snapshot->button_flags = (uint8_t)(read_bit(primary, 13) | read_bit(primary, 10) << 1 |
                                       (read_bit(primary, 12) | read_bit(extended, 14)) << 2 |
                                       read_bit(primary, 9) << 3);
    if (state->wheel_mode == WHEEL_MODE_CRC || state->wheel_mode == WHEEL_MODE_CRC_AUTHENTICATED) {
        snapshot->button_flags |=
            (uint8_t)(read_bit(extended, 7) << 4 | read_bit(extended, 6) << 5);
        snapshot->packed_buttons =
            (uint8_t)((state->controls[7] & 1u) << 4 | (state->controls[7] & 4u) << 3);
    } else if (state->wheel_mode == WHEEL_MODE_NINE || state->wheel_mode == WHEEL_MODE_ELEVEN ||
               state->wheel_mode == WHEEL_MODE_EXTENDED_BUTTON) {
        snapshot->packed_buttons =
            (uint8_t)(read_bit(extended, 14) | read_bit(extended, 15) << 1 |
                      read_bit(extended, 1) << 2 | read_bit(extended, 4) << 3);
    } else if (state->wheel_mode == WHEEL_MODE_TEN) {
        snapshot->packed_buttons =
            (uint8_t)(read_bit(extended, 14) | read_bit(extended, 15) << 1 |
                      read_bit(extended, 11) << 2 | read_bit(extended, 12) << 3 |
                      (state->controls[6] & 3u) << 4);
    } else {
        snapshot->packed_buttons = (uint8_t)((state->controls[6] & 3u) << 4);
    }
    snapshot->extended_button =
        state->wheel_mode == WHEEL_MODE_EXTENDED_BUTTON ? read_bit(extended, 10) : 0;
}

/**
 * @brief Initializes Xbox GIP input composition.
 *
 * Clears the alternating packet state so the first built snapshot asserts its packet bit.
 *
 * @param[out] builder Input builder to initialize.
 */
void usb_xbox_gip_input_builder_init(UsbXboxGipInputBuilder *builder) {
    *builder = (UsbXboxGipInputBuilder){0};
}

/**
 * @brief Builds a logical Xbox GIP input snapshot.
 *
 * Maps normalized wheel buttons and extension controls, copies the live steering and pedal axes,
 * lays out three rotary selector groups around the signed encoder event, and scales the active
 * force-feedback percentage to the protocol byte range.
 *
 * @param[in,out] builder Input builder containing the alternating packet state.
 * @param[in] state Current normalized wheel, pedal, profile, and shifter state.
 * @param[out] snapshot Logical Xbox GIP snapshot.
 */
void usb_xbox_gip_input_build(UsbXboxGipInputBuilder *builder, const UsbXboxGipInputState *state,
                              UsbXboxGipInputSnapshot *snapshot) {
    *snapshot = (UsbXboxGipInputSnapshot){0};
    map_primary_buttons(builder, state, snapshot);
    map_extended_buttons(state, snapshot);
    snapshot->steering = state->steering;
    memcpy(snapshot->pedals, state->pedals, sizeof(snapshot->pedals));
    snapshot->auxiliary_pedal = state->auxiliary_pedal;
    snapshot->axis_mode = state->axis_mode;
    snapshot->led_state = state->led_state;
    snapshot->steering_range_degrees = state->steering_range_degrees;
    snapshot->force_feedback_level =
        (uint8_t)((uint16_t)state->force_feedback_percent * UINT8_MAX / 100u);
    memcpy(snapshot->pedal_active, state->pedal_active, sizeof(snapshot->pedal_active));
    snapshot->auxiliary_pedal_active = state->auxiliary_pedal_active;
    memcpy(snapshot->clutch_paddles, state->clutch_paddles, sizeof(snapshot->clutch_paddles));
    snapshot->selectors[0] = state->rotary[0];
    snapshot->selectors[1] = state->rotary[1];
    snapshot->selectors[2] = state->rotary[2];
    snapshot->selectors[3] = state->encoder_direction > 0 ? 2 : state->encoder_direction < 0;
    snapshot->selectors[4] = state->rotary[3];
    snapshot->selectors[5] = state->rotary[4];
}
