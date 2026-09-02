#include "usb/xbox_gip_input.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/** @brief Wheel modes with mode-specific Xbox GIP input mappings. */
enum {
    WHEEL_MODE_VENDOR_FOUR = 4,        /**< Wheel mode four. */
    WHEEL_MODE_CRC = 6,                /**< CRC wheel mode. */
    WHEEL_MODE_VENDOR_C = 12,          /**< Wheel mode twelve. */
    WHEEL_MODE_NINE = 9,               /**< Wheel mode 9. */
    WHEEL_MODE_TEN = 10,               /**< Wheel mode 10. */
    WHEEL_MODE_ELEVEN = 11,            /**< Wheel mode 11. */
    WHEEL_MODE_CRC_AUTHENTICATED = 21, /**< Authenticated CRC wheel mode. */
    WHEEL_MODE_EXTENDED_BUTTON = 29,   /**< Wheel mode with a dedicated extended button. */
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
 * @brief Merges one adapter bit into a GIP button byte.
 *
 * Adapter button mappings are OR-merged by the official vendor status builder.
 *
 * @param[in,out] target GIP button byte receiving the bit.
 * @param[in] bit Destination bit index.
 * @param[in] value Source bit value.
 */
static void merge_adapter_bit(uint8_t *target, uint8_t bit, uint8_t value) {
    *target |= (uint8_t)((value & 1u) << bit);
}

/**
 * @brief Merges adapter face buttons into the GIP secondary button byte.
 *
 * Maps adapter byte zero's four face-button bits into the low nibble used by the Xbox report.
 *
 * @param[in,out] snapshot GIP snapshot receiving button bits.
 * @param[in] adapter Attached adapter input.
 */
static void merge_adapter_face_buttons(UsbXboxGipInputSnapshot *snapshot,
                                       const WheelAdapterInput *adapter) {
    merge_adapter_bit(&snapshot->buttons[1], 0, read_bit(adapter->buttons[0], 0));
    merge_adapter_bit(&snapshot->buttons[1], 1, read_bit(adapter->buttons[0], 3));
    merge_adapter_bit(&snapshot->buttons[1], 2, read_bit(adapter->buttons[0], 1));
    merge_adapter_bit(&snapshot->buttons[1], 3, read_bit(adapter->buttons[0], 2));
}

/**
 * @brief Merges adapter buttons for adapter mode zero.
 *
 * Applies the standard endpoint mapping and clears suppressed face-button fields.
 *
 * @param[in,out] snapshot GIP snapshot receiving button bits.
 * @param[in] adapter Attached adapter input.
 * @param[in] suppress_base_buttons True while tuning interaction suppresses face-button output.
 */
static void merge_adapter_mode_zero(UsbXboxGipInputSnapshot *snapshot,
                                    const WheelAdapterInput *adapter, bool suppress_base_buttons) {
    merge_adapter_bit(&snapshot->buttons[1], 4,
                      read_bit(adapter->buttons[0], 6) | read_bit(adapter->buttons[0], 5));
    merge_adapter_bit(&snapshot->buttons[1], 5, read_bit(adapter->buttons[0], 4));
    merge_adapter_bit(&snapshot->buttons[0], 3, read_bit(adapter->buttons[1], 0));
    merge_adapter_bit(&snapshot->buttons[0], 5, read_bit(adapter->buttons[1], 2));
    merge_adapter_bit(&snapshot->buttons[0], 7, read_bit(adapter->buttons[1], 1));
    merge_adapter_bit(&snapshot->buttons[0], 6, read_bit(adapter->buttons[1], 4));
    merge_adapter_bit(&snapshot->buttons[0], 2, read_bit(adapter->buttons[1], 5));
    if (suppress_base_buttons) {
        snapshot->buttons[1] &= 0xf0u;
        snapshot->buttons[0] &= (uint8_t)~0x10u;
        return;
    }
    merge_adapter_face_buttons(snapshot, adapter);
    merge_adapter_bit(&snapshot->buttons[0], 4, read_bit(adapter->buttons[1], 3));
}

/**
 * @brief Merges adapter buttons for adapter mode one.
 *
 * Applies the extended endpoint mapping and clears suppressed face-button fields.
 *
 * @param[in,out] snapshot GIP snapshot receiving button bits.
 * @param[in] adapter Attached adapter input.
 * @param[in] suppress_base_buttons True while tuning interaction suppresses face-button output.
 */
static void merge_adapter_mode_one(UsbXboxGipInputSnapshot *snapshot,
                                   const WheelAdapterInput *adapter, bool suppress_base_buttons) {
    merge_adapter_bit(&snapshot->buttons[0], 3, read_bit(adapter->buttons[0], 7));
    merge_adapter_bit(&snapshot->buttons[0], 5, read_bit(adapter->buttons[1], 2));
    merge_adapter_bit(&snapshot->buttons[0], 7, read_bit(adapter->buttons[0], 4));
    merge_adapter_bit(&snapshot->buttons[0], 6, read_bit(adapter->buttons[0], 6));
    merge_adapter_bit(&snapshot->buttons[0], 4,
                      read_bit(adapter->buttons[2], 3) | read_bit(adapter->buttons[2], 2) |
                          read_bit(adapter->buttons[1], 1));
    merge_adapter_bit(&snapshot->buttons[0], 2, read_bit(adapter->buttons[1], 0));
    if (suppress_base_buttons) {
        snapshot->buttons[1] &= 0xf0u;
        snapshot->buttons[0] &= (uint8_t)~0x10u;
        return;
    }
    merge_adapter_face_buttons(snapshot, adapter);
}

/**
 * @brief Merges adapter buttons for an unknown adapter mode.
 *
 * Applies the fallback mapping and places adapter mode bits in the packed-button field used by
 * the official report builder.
 *
 * @param[in,out] snapshot GIP snapshot receiving button bits.
 * @param[in] adapter Attached adapter input.
 * @param[in] suppress_base_buttons True while tuning interaction suppresses face-button output.
 */
static void merge_adapter_fallback(UsbXboxGipInputSnapshot *snapshot,
                                   const WheelAdapterInput *adapter, bool suppress_base_buttons) {
    if (read_bit(adapter->buttons[0], 5) != 0) {
        snapshot->packed_buttons |= 0x10u;
        snapshot->buttons[1] &= (uint8_t)~0x10u;
    }
    if (read_bit(adapter->buttons[0], 4) != 0) {
        snapshot->packed_buttons |= 0x20u;
        snapshot->buttons[1] &= (uint8_t)~0x20u;
    }
    merge_adapter_bit(&snapshot->buttons[0], 3, read_bit(adapter->buttons[1], 0));
    merge_adapter_bit(&snapshot->buttons[0], 5, read_bit(adapter->buttons[1], 2));
    merge_adapter_bit(&snapshot->buttons[0], 7, read_bit(adapter->buttons[1], 1));
    merge_adapter_bit(&snapshot->buttons[0], 6, read_bit(adapter->buttons[1], 4));
    merge_adapter_bit(&snapshot->buttons[0], 2, read_bit(adapter->buttons[1], 5));
    if (suppress_base_buttons) {
        snapshot->buttons[1] &= 0xf0u;
        return;
    }
    merge_adapter_bit(&snapshot->buttons[0], 4,
                      read_bit(adapter->buttons[2], 2) | read_bit(adapter->buttons[2], 3) |
                          read_bit(adapter->buttons[1], 3));
    merge_adapter_face_buttons(snapshot, adapter);
}

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
    snapshot->buttons[1] |= (uint8_t)(state->shifter_transitions[1] ? 0x10u : 0u);
    snapshot->buttons[1] |= (uint8_t)(state->shifter_transitions[0] ? 0x20u : 0u);
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
 * Packs common secondary-input flags, mode-specific control bits, and the dedicated mode-29
 * extension input.
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

void usb_xbox_gip_input_builder_init(UsbXboxGipInputBuilder *builder) {
    *builder = (UsbXboxGipInputBuilder){0};
}

static uint8_t encode_led(UsbXboxGipInputBuilder *builder, uint8_t value, uint8_t axis_mode) {
    if (axis_mode != 1) {
        builder->led_encoded = 0;
        return 0;
    }
    if (builder->led_input == value) {
        return builder->led_encoded;
    }
    builder->led_input = value;
    if (value == 0) {
        builder->led_encoded = 0;
    } else if (value == 1) {
        builder->led_encoded = UINT8_MAX;
    } else if ((value & (uint8_t)(value - 1u)) == 0) {
        uint8_t encoded = 0;
        while (value > 1) {
            value >>= 1;
            encoded++;
        }
        builder->led_encoded = encoded;
    }
    return builder->led_encoded;
}

void usb_xbox_gip_input_build(UsbXboxGipInputBuilder *builder, const UsbXboxGipInputState *state,
                              UsbXboxGipInputSnapshot *snapshot) {
    *snapshot = (UsbXboxGipInputSnapshot){0};
    map_primary_buttons(builder, state, snapshot);
    map_extended_buttons(state, snapshot);
    snapshot->steering = state->steering;
    memcpy(snapshot->pedals, state->pedals, sizeof(snapshot->pedals));
    snapshot->auxiliary_pedal = state->auxiliary_pedal;
    snapshot->axis_mode = state->axis_mode;
    snapshot->led_state = encode_led(builder, state->led_state, state->axis_mode);
    snapshot->steering_range_units = state->steering_range_units;
    snapshot->force_feedback_level = state->force_feedback_level;
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

void usb_xbox_gip_input_merge_adapter_buttons(UsbXboxGipInputSnapshot *snapshot, uint8_t wheel_mode,
                                              const WheelAdapterInput *adapter,
                                              bool suppress_base_buttons) {
    if (snapshot == 0 || adapter == 0 || !adapter->connected ||
        (wheel_mode != WHEEL_MODE_VENDOR_FOUR && wheel_mode != WHEEL_MODE_CRC &&
         wheel_mode != WHEEL_MODE_VENDOR_C && wheel_mode != WHEEL_MODE_CRC_AUTHENTICATED)) {
        return;
    }
    if (adapter->mode == 0) {
        merge_adapter_mode_zero(snapshot, adapter, suppress_base_buttons);
    } else if (adapter->mode == 1) {
        merge_adapter_mode_one(snapshot, adapter, suppress_base_buttons);
    } else {
        merge_adapter_fallback(snapshot, adapter, suppress_base_buttons);
    }
}
