#include "usb/fanatec_input.h"

#include <stddef.h>
#include <string.h>

/** @brief Fanatec report offsets, status masks, and multi-position mode constants. */
enum {
    BUTTONS_OFFSET = 0,                          /**< Button-bank payload offset. */
    ROTARY_OFFSET = 5,                           /**< Rotary payload offset. */
    ACCESSORY_OFFSET = 10,                       /**< Accessory payload offset. */
    TRANSFER_CODE_OFFSET = 15,                   /**< Transfer-code payload offset. */
    STEERING_OFFSET = 16,                        /**< Steering payload offset. */
    PEDALS_OFFSET = 18,                          /**< Pedal payload offset. */
    CLUTCH_PADDLES_OFFSET = 24,                  /**< Clutch-paddle payload offset. */
    AUXILIARY_PEDAL_OFFSET = 26,                 /**< Auxiliary-pedal payload offset. */
    ENCODER_OFFSET = 27,                         /**< Encoder-position payload offset. */
    STATUS_OFFSET = 28,                          /**< Status payload offset. */
    MODE_OFFSET = 29,                            /**< Wheel-mode payload offset. */
    AXIS_LIMIT_OFFSET = 30,                      /**< Axis-limit payload offset. */
    USAGE_PAGE_OFFSET = 31,                      /**< Button usage-page payload offset. */
    USAGE_OFFSET = 32,                           /**< Button usage payload offset. */
    BUTTON_USAGE_PAGE = 9,                       /**< HID button usage page value. */
    BUTTON_USAGE = 3,                            /**< HID button usage value. */
    BITE_POINT_UPDATE_OFFSET = 30,               /**< Native bite-point update offset. */
    BITE_POINT_UPDATE_MARKER = 0xff,             /**< Native bite-point update marker. */
    BITE_POINT_UPDATE_TYPE = 2,                  /**< Native bite-point update type. */
    SHIFTER_TRANSITION_BUTTON_BANK = 1,          /**< H-pattern transition button bank. */
    SHIFTER_GEAR_BUTTON_BANK = 2,                /**< H-pattern gear button bank. */
    SHIFTER_SEQUENTIAL_BUTTON_BANK = 4,          /**< Sequential shifter button bank. */
    SHIFTER_TRANSITION_BUTTON_0 = 1 << 0,        /**< Primary transition button bit. */
    SHIFTER_TRANSITION_BUTTON_1 = 1 << 1,        /**< Secondary transition button bit. */
    STATUS_SEQUENTIAL_SHIFTERS = 1 << 0,         /**< Status bit for all-sequential shifters. */
    STATUS_THERMAL_EFFECT_LIMIT = 1 << 4,        /**< Status bit for thermal effect limiting. */
    STATUS_WHEEL_CALIBRATION_AVAILABLE = 1 << 6, /**< Status bit for wheel calibration support. */
    STATUS_WHEEL_INPUT_CAPABILITY = 1 << 7,      /**< Status bit for wheel input capability. */
    MULTI_POSITION_ENCODER_MODE = 0,             /**< Multi-position encoder-event mode. */
    MULTI_POSITION_PULSE_MODE = 1,               /**< Multi-position pulse mode. */
    MULTI_POSITION_CONSTANT_MODE = 2,            /**< Multi-position constant-position mode. */
};

/**
 * @brief Writes a little-endian sixteen-bit value.
 *
 * Stores the low byte first and the high byte second.
 *
 * @param[out] destination Two-byte destination.
 * @param[in] value Value to write.
 */
static void write_u16(uint8_t *destination, uint16_t value) {
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8);
}

static uint8_t fanatec_map_bit(uint8_t destination, uint8_t destination_bit, uint8_t source,
                               uint8_t source_bit) {
    uint8_t mask = (uint8_t)(1u << destination_bit);
    uint8_t value = (uint8_t)((source >> source_bit) & 1u);
    return (uint8_t)((destination & (uint8_t)~mask) | (uint8_t)(value << destination_bit));
}

static uint8_t fanatec_merge_bit(uint8_t destination, uint8_t destination_bit, uint8_t source,
                                 uint8_t source_bit) {
    return (uint8_t)(destination | (uint8_t)(((source >> source_bit) & 1u) << destination_bit));
}

static uint8_t fanatec_remap_button_nibble(uint8_t value) {
    static const uint8_t map[16] = {8, 2, 6, 8, 4, 3, 5, 0, 0, 1, 7, 0, 8, 0, 2, 5};
    uint8_t index =
        (uint8_t)(((value & 1u) << 3) | ((value >> 1) & 4u) | (value & 2u) | ((value >> 2) & 1u));
    return map[index];
}

static uint16_t fanatec_remap_axis_bits(uint16_t value) {
    uint16_t upper = (uint16_t)((value >> 2) & 0x0010u);
    upper |= (uint16_t)((value >> 2) & 0x0100u);
    upper |= (uint16_t)((value >> 2) & 0x0001u);
    upper |= (uint16_t)(((value >> 14) & 1u) << 12);
    upper |= value & 0x2222u;

    uint16_t middle = (uint16_t)((value >> 1) & 0x0040u);
    middle |= (uint16_t)((value >> 1) & 0x0004u);
    middle |= (uint16_t)(((value >> 11) & 1u) << 10);
    middle |= (uint16_t)((value >> 15) << 14);

    uint16_t lower = (uint16_t)((value << 3) & 0x0080u);
    lower |= (uint16_t)((value & 1u) << 3);
    lower |= (uint16_t)(((value >> 8) & 1u) << 11);
    lower |= (uint16_t)((value >> 12) << 15);
    return (uint16_t)(upper | middle | lower);
}

static uint16_t fanatec_remap_reduced_axis_bits(uint16_t value) {
    return (uint16_t)(((value >> 2) & 0x100u) | (((value >> 14) & 1u) << 12) | ((value >> 2) & 1u) |
                      (value & 0x2202u) | ((value >> 1) & 0x40u) | ((value >> 1) & 4u) |
                      (((value >> 11) & 1u) << 10) | (((value >> 15) & 1u) << 14) |
                      ((value << 3) & 0x80u) | ((value & 1u) << 3) | (((value >> 8) & 1u) << 11) |
                      ((value >> 12) << 15));
}

static void fanatec_filter_bytes(uint8_t history[FANATEC_INPUT_HISTORY_DEPTH][3], uint8_t index,
                                 uint8_t value[3]) {
    for (uint8_t byte = 0; byte < 3; byte++) {
        history[index][byte] = value[byte];
        value[byte] = (uint8_t)(history[0][byte] & history[1][byte] & history[2][byte]);
    }
}

void fanatec_input_pipeline_init(fanatec_input_pipeline_state *pipeline) {
    if (pipeline != NULL) {
        memset(pipeline, 0, sizeof(*pipeline));
    }
}

void fanatec_input_pipeline_filter(fanatec_input_pipeline_state *pipeline,
                                   fanatec_input_source *source) {
    if (pipeline == NULL || source == NULL) {
        return;
    }

    uint8_t index = pipeline->history_index;
    fanatec_filter_bytes(pipeline->primary_history, index, source->buttons);
    uint8_t secondary[3] = {source->secondary_buttons, source->packed_rotary_positions,
                            source->accessory};
    if (source->mode == 0x10) {
        fanatec_filter_bytes(pipeline->secondary_history, index, secondary);
        source->secondary_buttons = secondary[0];
        source->packed_rotary_positions = secondary[1];
        source->accessory = secondary[2];
    }
    pipeline->history_index = (uint8_t)((index + 1u) % FANATEC_INPUT_HISTORY_DEPTH);
}

static void fanatec_map_primary_buttons(fanatec_input_state *state,
                                        const fanatec_input_source *source) {
    uint8_t *hat = &state->button_banks[0];
    uint8_t *first = &state->button_banks[1];
    uint8_t *third = &state->button_banks[3];
    uint8_t *fourth = &state->button_banks[4];
    uint8_t mode = source->mode;

    memset(state->button_banks, 0, sizeof(state->button_banks));
    *hat = fanatec_remap_button_nibble(source->buttons[0]);
    *first = fanatec_map_bit(*first, 1, source->buttons[1], 3);
    *first = fanatec_map_bit(*first, 3, source->buttons[1], 4);
    *first = fanatec_map_bit(*first, 0, source->buttons[1], 0);

    if (mode != 0x0e) {
        *hat = fanatec_map_bit(*hat, 4, source->buttons[0], 5);
        *hat = fanatec_map_bit(*hat, 5, source->buttons[0], 4);
        *hat = fanatec_map_bit(*hat, 7, source->buttons[0], 7);
        *hat = fanatec_map_bit(*hat, 6, source->buttons[0], 6);
        *first = fanatec_map_bit(*first, 7, source->buttons[1], 5);
        *first = fanatec_map_bit(*first, 2, source->buttons[1], 1);
        *first = fanatec_map_bit(*first, 6, source->buttons[1], 2);
        *first = fanatec_map_bit(*first, 4, source->buttons[1], 6);
        *first = fanatec_map_bit(*first, 5, source->buttons[1], 7);
    }

    switch (mode) {
    case 0x0a:
    case 0x1b:
    case 0x1c:
    case 0x0f:
    case 0x17:
        *first = fanatec_map_bit(*first, 0, source->buttons[1], 0);
        *first = fanatec_map_bit(*first, 1, source->buttons[1], 3);
        if (mode == 0x1b) {
            *third = fanatec_map_bit(*third, 1, source->buttons[2], 1);
        } else {
            *third = fanatec_map_bit(*third, 1, source->buttons[2], 0);
            *third = fanatec_map_bit(*third, 4, source->buttons[2], 1);
        }
        *third = fanatec_map_bit(*third, 0, source->pulse_flags[0], 0);
        *third = fanatec_map_bit(*third, 5, source->buttons[2], 2);
        *third = fanatec_map_bit(*third, 7, source->buttons[2], 5);
        *third = fanatec_map_bit(*third, 6, source->pulse_flags[0], 1);
        break;
    case 0x10:
        *third = fanatec_map_bit(*third, 1, source->buttons[2], 0);
        *third = fanatec_map_bit(*third, 4, source->buttons[2], 1);
        break;
    case 0x11:
        *hat = fanatec_map_bit(*hat, 5, source->buttons[2], 1);
        *first = fanatec_map_bit(*first, 0, source->buttons[1], 3);
        *first = fanatec_map_bit(*first, 1, source->buttons[1], 0);
        *third = fanatec_map_bit(*third, 1, source->buttons[2], 0);
        break;
    case 0x13:
    case 0x14:
        *third = fanatec_map_bit(*third, 4, source->buttons[2], 1);
        *third = fanatec_map_bit(*third, 5, source->buttons[2], 2);
        *third = fanatec_map_bit(*third, 1, source->buttons[2], 0);
        *third = fanatec_map_bit(*third, 6, source->buttons[2], 4);
        break;
    case 0x15:
        *third = fanatec_map_bit(*third, 4, source->buttons[2], 1);
        *third = fanatec_map_bit(*third, 1, source->buttons[2], 3);
        *third = fanatec_map_bit(*third, 5, source->buttons[2], 2);
        break;
    case 0x16:
        *third = fanatec_map_bit(*third, 4, source->buttons[2], 1);
        *third = fanatec_map_bit(*third, 5, source->buttons[2], 2);
        *third = fanatec_map_bit(*third, 1, source->buttons[2], 0);
        break;
    case 0x05:
    case 0x07:
    case 0x08:
        *third = fanatec_map_bit(*third, 0, source->buttons[2], 0);
        *third = fanatec_map_bit(*third, 1, source->buttons[2], 1);
        *third = fanatec_map_bit(*third, 6, source->buttons[2], 3);
        *third = fanatec_map_bit(*third, 4, source->buttons[2], 2);
        break;
    case 0x0e:
        *third = fanatec_map_bit(*third, 5, source->buttons[2], 0);
        *third = fanatec_map_bit(*third, 4, source->buttons[2], 1);
        *hat = fanatec_map_bit(*hat, 5, source->buttons[0], 5);
        *hat = fanatec_map_bit(*hat, 6, source->buttons[0], 7);
        *first = fanatec_map_bit(*first, 2, source->buttons[0], 6);
        *hat = fanatec_map_bit(*hat, 7, source->buttons[1], 5);
        *hat = fanatec_map_bit(*hat, 4, source->buttons[1], 1);
        *first = fanatec_map_bit(*first, 6, source->buttons[1], 2);
        *first = fanatec_map_bit(*first, 4, source->buttons[1], 6);
        *first = fanatec_map_bit(*first, 5, source->buttons[1], 7);
        *third = fanatec_map_bit(*third, 7, source->buttons[2], 5);
        *third = fanatec_map_bit(*third, 0, source->pulse_flags[0], 0);
        *third = fanatec_map_bit(*third, 6, source->pulse_flags[0], 1);
        break;
    default:
        *third = fanatec_map_bit(*third, 0, source->buttons[2], 0);
        *third = fanatec_map_bit(*third, 4, source->buttons[2], 1);
        *third = fanatec_map_bit(*third, 5, source->buttons[2], 2);
        *third = fanatec_map_bit(*third, 1, source->buttons[2], 3);
        *third = fanatec_map_bit(*third, 6, source->buttons[2], 4);
        break;
    }

    *third = fanatec_map_bit(*third, 7, source->buttons[2], 5);
    if (mode == 0x09 || mode == 0x0b || mode == 0x1d ||
        (mode == 0x0c && !source->adapter_connected)) {
        *fourth = fanatec_map_bit(*fourth, 2, source->buttons[2], 6);
        *fourth = fanatec_map_bit(*fourth, 3, source->buttons[2], 7);
        *fourth = fanatec_map_bit(*fourth, 4, source->buttons[1], 1);
        *fourth = fanatec_map_bit(*fourth, 5, source->buttons[1], 4);
        *first &= 0xf3u;
    } else if (mode == 0x0a || mode == 0x1c || mode == 0x1b || mode == 0x0f || mode == 0x17 ||
               mode == 0x0e) {
        *fourth = fanatec_map_bit(*fourth, 2, source->buttons[2], 6);
        *fourth = fanatec_map_bit(*fourth, 3, source->buttons[2], 7);
        *fourth = fanatec_map_bit(*fourth, 4, source->buttons[2], 3);
        *fourth = fanatec_map_bit(*fourth, 5, source->buttons[2], 4);
        *fourth = fanatec_map_bit(*fourth, 6, source->pulse_flags[0], 2);
        *fourth = fanatec_map_bit(*fourth, 7, source->pulse_flags[0], 3);
    } else if (mode == 0x11) {
        *fourth = fanatec_map_bit(*fourth, 2, source->buttons[2], 6);
        *fourth = fanatec_map_bit(*fourth, 3, source->buttons[2], 7);
        *fourth = fanatec_map_bit(*fourth, 4, source->buttons[2], 3);
        *fourth = fanatec_map_bit(*fourth, 5, source->buttons[2], 4);
        *first = fanatec_map_bit(*first, 2, source->buttons[1], 2);
        *first = fanatec_map_bit(*first, 6, source->buttons[1], 1);
    } else if (mode == 0x10) {
        *fourth &= 0x03u;
    } else {
        *fourth = fanatec_map_bit(*fourth, 2, source->auxiliary_buttons, 0);
        *fourth = fanatec_map_bit(*fourth, 3, source->auxiliary_buttons, 1);
        *fourth &= 0x0fu;
    }
}

static void fanatec_map_adapter_buttons(fanatec_input_state *state,
                                        const fanatec_input_source *source) {
    if (!source->adapter_connected || (source->adapter_mode != 0 && source->adapter_mode != 1)) {
        return;
    }
    uint8_t *hat = &state->button_banks[0];
    uint8_t *first = &state->button_banks[1];
    uint8_t *third = &state->button_banks[3];
    uint8_t *fourth = &state->button_banks[4];
    const uint8_t *adapter = source->adapter_buttons;
    uint8_t auxiliary = source->auxiliary_buttons;
    if (source->adapter_mode == 0) {
        *first = fanatec_merge_bit(*first, 3, adapter[0], 4);
        *first = fanatec_merge_bit(*first, 7, adapter[0], 5);
        *first = fanatec_merge_bit(*first, 2, adapter[0], 6);
        *first = fanatec_merge_bit(*first, 6, adapter[0], 7);
        *first = fanatec_merge_bit(*first, 4, adapter[1], 0);
        *hat = fanatec_merge_bit(*hat, 6, adapter[1], 1);
        *hat = fanatec_merge_bit(*hat, 7, adapter[1], 2);
        *hat = fanatec_merge_bit(*hat, 4, adapter[1], 3);
        *hat = fanatec_merge_bit(*hat, 5, adapter[1], 4);
        *first = fanatec_merge_bit(*first, 5, adapter[1], 5);
        *third = fanatec_merge_bit(*third, 4, adapter[2], 3);
        *fourth = fanatec_map_bit(*fourth, 5, adapter[1], 6);
        *fourth = fanatec_map_bit(*fourth, 3, adapter[1], 7);
        *fourth = fanatec_merge_bit(*fourth, 3, auxiliary, 1);
        *fourth = fanatec_map_bit(*fourth, 4, adapter[2], 0);
        *fourth = fanatec_map_bit(*fourth, 2, adapter[2], 1);
        *fourth = fanatec_merge_bit(*fourth, 2, auxiliary, 0);
        *third = fanatec_merge_bit(*third, 5, adapter[2], 2);
        *third = fanatec_map_bit(*third, 7, adapter[2], 4);
    } else {
        *hat = fanatec_merge_bit(*hat, 7, adapter[0], 4);
        *first = fanatec_merge_bit(*first, 3, adapter[0], 5);
        *hat = fanatec_merge_bit(*hat, 4, adapter[0], 6);
        *first = fanatec_merge_bit(*first, 4, adapter[0], 7);
        *first = fanatec_merge_bit(*first, 5, adapter[1], 0);
        *hat = fanatec_merge_bit(*hat, 5, adapter[1], 1);
        *hat = fanatec_merge_bit(*hat, 6, adapter[1], 2);
        *first = fanatec_merge_bit(*first, 2, adapter[1], 3);
        *third = fanatec_merge_bit(*third, 1, adapter[1], 4);
        *third = fanatec_merge_bit(*third, 4, adapter[2], 3);
        *first = fanatec_merge_bit(*first, 7, adapter[2], 1);
        *first = fanatec_merge_bit(*first, 7, auxiliary, 0);
        *first = fanatec_merge_bit(*first, 6, adapter[1], 6);
        *third = fanatec_map_bit(*third, 5, adapter[2], 2);
    }
}

void fanatec_input_pipeline_map(fanatec_input_state *state, const fanatec_input_source *source) {
    if (state == NULL || source == NULL) {
        return;
    }

    if (source->protocol_active) {
        memset(state->rotary, 0, sizeof(state->rotary));
        memset(state->accessory, 0, sizeof(state->accessory));
    }

    fanatec_map_primary_buttons(state, source);
    if (source->neutral_shifter_axes) {
        state->button_banks[2] = 0;
        state->button_banks[4] = fanatec_map_bit(state->button_banks[4], 0, source->hat, 0);
        state->button_banks[4] = fanatec_map_bit(state->button_banks[4], 1, source->hat, 1);
    } else {
        state->button_banks[2] = source->hat;
        state->button_banks[4] &= 0xfcu;
    }

    if (source->protocol_active) {
        state->rotary[0] = source->rotary_positions[0];
        state->rotary[1] = source->rotary_positions[1];
        state->rotary[3] |= source->extended_buttons[1];
    } else {
        state->rotary[2] = source->extended_buttons[0];
        state->rotary[3] = source->extended_buttons[1];
        state->rotary[4] = source->extended_buttons[2];
        state->accessory[0] = source->extended_buttons[3];
    }
    state->accessory[4] = (uint8_t)((state->accessory[4] & 0xf0u) | (source->accessory & 0x0fu));
    fanatec_map_adapter_buttons(state, source);

    state->steering = source->steering;
    memcpy(state->pedals, source->pedals, sizeof(state->pedals));
    state->auxiliary_pedal = source->auxiliary_pedal;
    memcpy(state->clutch_paddles, source->clutch_paddles, sizeof(state->clutch_paddles));
    state->status_flags =
        (uint8_t)((source->status_flags & 0xfeu) | (source->neutral_shifter_axes ? 1u : 0u));
    state->status_flags = fanatec_map_bit(state->status_flags, 6, source->calibration_available, 0);
    state->status_flags = fanatec_map_bit(state->status_flags, 7, source->axis_report_enabled, 0);
    state->transfer_code = source->transfer_code;
    state->wheel_mode = source->mode;
    state->axis_limit = source->axis_limit;

    if (source->mode == 0x10) {
        uint16_t remapped = fanatec_remap_axis_bits(
            (uint16_t)source->secondary_buttons | ((uint16_t)source->packed_rotary_positions << 8));
        state->accessory[2] = (uint8_t)remapped;
        state->accessory[3] = (uint8_t)(remapped >> 8);
        state->accessory[1] =
            (uint8_t)((state->accessory[1] & 0x0fu) | (source->auxiliary_flags & 0xf0u));
    } else if (source->mode == 0x0e) {
        state->button_banks[1] =
            fanatec_map_bit(state->button_banks[1], 6, source->secondary_buttons, 6);
        state->button_banks[1] =
            fanatec_map_bit(state->button_banks[1], 7, source->secondary_buttons, 5);
        uint16_t remapped = fanatec_remap_reduced_axis_bits(
            (uint16_t)source->secondary_buttons | ((uint16_t)source->packed_rotary_positions << 8));
        state->accessory[2] = (uint8_t)remapped;
        state->accessory[3] = (uint8_t)(remapped >> 8);
    }

    if (source->mode == 0x1b) {
        state->accessory[2] |= (uint8_t)((source->pulse_flags[0] >> 3) & 0x08u);
        state->accessory[2] |= (uint8_t)((source->pulse_flags[0] >> 5) & 0x04u);
        state->accessory[3] = (uint8_t)((state->accessory[3] & 0x0fu) |
                                        (uint8_t)((source->pulse_flags[1] & 0x03u) << 6));
    }
    if (source->mode == 0x1c) {
        state->accessory[1] = fanatec_merge_bit(state->accessory[1], 5, source->auxiliary_flags, 2);
        state->accessory[1] = fanatec_merge_bit(state->accessory[1], 6, source->auxiliary_flags, 3);
        state->button_banks[3] =
            fanatec_map_bit(state->button_banks[3], 5, source->auxiliary_flags, 1);
        uint16_t remapped = fanatec_remap_axis_bits(source->secondary_buttons);
        state->accessory[2] =
            (uint8_t)((state->accessory[2] & 0x0fu) | ((uint8_t)remapped & 0xf0u));
        state->rotary[3] |= (uint8_t)((source->pulse_flags[0] >> 3) & 0x08u);
        state->rotary[3] |= (uint8_t)((source->pulse_flags[0] >> 5) & 0x04u);
    }
}

uint8_t fanatec_input_report_mode(uint8_t wheel_mode, bool command_invalid) {
    return command_invalid ? FANATEC_INPUT_DIRECT_DRIVE_MODE : wheel_mode;
}

void fanatec_input_pipeline_apply(fanatec_input_pipeline_state *pipeline,
                                  fanatec_input_state *state, const fanatec_input_source *source) {
    if (pipeline == NULL || state == NULL || source == NULL) {
        return;
    }
    fanatec_input_source filtered = *source;
    fanatec_input_pipeline_filter(pipeline, &filtered);
    fanatec_input_pipeline_map(state, &filtered);
}

void fanatec_input_apply_wheel_controls(fanatec_input_state *state, const uint8_t controls[8],
                                        bool include_extended) {
    state->rotary[0] = controls[0];
    state->rotary[1] = controls[1];
    if (!include_extended) {
        return;
    }
    state->rotary[2] = controls[2];
    state->rotary[3] = controls[3];
    state->rotary[4] = controls[4];
    state->accessory[0] = controls[5];
}

void fanatec_input_apply_quaternary_rotary_event(fanatec_input_state *state, uint8_t event) {
    state->accessory[0] = event;
}

void fanatec_input_apply_wheel_accessory(fanatec_input_state *state, uint8_t flags) {
    state->accessory[4] = (uint8_t)((state->accessory[4] & 0xf0u) | (flags & 0x0fu));
}

void fanatec_input_apply_alternative_shifter(fanatec_input_state *state, bool enabled) {
    if (enabled) {
        state->accessory[4] |= 0x80u;
    } else {
        state->accessory[4] &= 0x7fu;
    }
}

void fanatec_input_apply_multi_position_mode(fanatec_input_state *state, uint8_t mode) {
    const uint8_t mask = 0x30;
    state->accessory[4] = (uint8_t)((state->accessory[4] & (uint8_t)~mask) | ((mode << 4) & mask));
}

/**
 * @brief Converts a rotary position to its report selector.
 *
 * Produces one bit for positions one through twelve. The alternate layout moves positions one
 * through four behind positions five through twelve.
 *
 * @param[in] position One-based rotary position.
 * @param[in] remap True to use the alternate selector layout.
 * @return Twelve-bit selector, or zero when the position is outside the supported range.
 */
static uint16_t multi_position_selector(uint8_t position, bool remap) {
    if (position == 0 || position > 12) {
        return 0;
    }

    uint16_t selector = (uint16_t)(1u << (position - 1));
    if (!remap) {
        return selector;
    }
    return selector <= 8 ? (uint16_t)(selector << 8) : (uint16_t)(selector >> 4);
}

/**
 * @brief Packs one rotary selector into the Fanatec rotary fields.
 *
 * Places the selected channel in its twelve-bit report slot without changing bits outside that
 * slot.
 *
 * @param[in,out] state Input report state to update.
 * @param[in] channel Rotary channel index.
 * @param[in] selector Twelve-bit one-hot selector.
 */
static void apply_multi_position_selector(fanatec_input_state *state, uint8_t channel,
                                          uint16_t selector) {
    if (channel == 0) {
        state->rotary[0] = (uint8_t)selector;
        state->rotary[1] |= (uint8_t)((selector >> 8) & 0x0fu);
    } else if (channel == 1) {
        state->rotary[1] |= (uint8_t)((selector << 4) & 0xf0u);
        state->rotary[2] = (uint8_t)(selector >> 4);
    } else {
        state->rotary[3] |= (uint8_t)((selector << 4) & 0xf0u);
        state->rotary[4] = (uint8_t)(selector >> 4);
    }
}

void fanatec_input_apply_multi_position_rotaries(fanatec_input_state *state, uint8_t mode,
                                                 const fanatec_multi_position_input *input) {
    memset(state->rotary, 0, sizeof(state->rotary));

    for (uint8_t channel = 0; channel < FANATEC_INPUT_MULTI_POSITION_CHANNELS; ++channel) {
        const fanatec_multi_position_channel *source = &input->channels[channel];
        if (!source->active) {
            continue;
        }
        if (mode == MULTI_POSITION_ENCODER_MODE) {
            if (channel == 0) {
                state->rotary[0] = source->event;
            } else if (channel == 1) {
                state->rotary[1] = (uint8_t)(source->event << 4);
            } else {
                state->rotary[3] = (uint8_t)(source->event << 4);
            }
            continue;
        }
        if (mode != MULTI_POSITION_CONSTANT_MODE &&
            (mode != MULTI_POSITION_PULSE_MODE || source->event == 0)) {
            continue;
        }
        apply_multi_position_selector(
            state, channel, multi_position_selector(source->position, input->remap_selectors));
    }
}

void fanatec_input_apply_shifter(fanatec_input_state *state, const ShifterInputState *shifter,
                                 ShifterGear gear) {
    const bool sequential_only = shifter->primary_mode == SHIFTER_INPUT_SEQUENTIAL &&
                                 shifter->secondary_mode == SHIFTER_INPUT_SEQUENTIAL;
    state->status_flags = (uint8_t)((state->status_flags & (uint8_t)~STATUS_SEQUENTIAL_SHIFTERS) |
                                    (sequential_only ? STATUS_SEQUENTIAL_SHIFTERS : 0));
    if (sequential_only) {
        uint8_t transitions = (shifter->primary_transition ? SHIFTER_TRANSITION_BUTTON_1 : 0) |
                              (shifter->secondary_transition ? SHIFTER_TRANSITION_BUTTON_0 : 0);
        state->button_banks[SHIFTER_GEAR_BUTTON_BANK] = 0;
        state->button_banks[SHIFTER_SEQUENTIAL_BUTTON_BANK] =
            (state->button_banks[SHIFTER_SEQUENTIAL_BUTTON_BANK] &
             (uint8_t)~(SHIFTER_TRANSITION_BUTTON_0 | SHIFTER_TRANSITION_BUTTON_1)) |
            transitions;
        return;
    }

    state->button_banks[SHIFTER_GEAR_BUTTON_BANK] = (uint8_t)gear;
    state->button_banks[SHIFTER_SEQUENTIAL_BUTTON_BANK] &=
        (uint8_t)~(SHIFTER_TRANSITION_BUTTON_0 | SHIFTER_TRANSITION_BUTTON_1);
    if (shifter->primary_transition) {
        state->button_banks[SHIFTER_TRANSITION_BUTTON_BANK] |= SHIFTER_TRANSITION_BUTTON_0;
    }
    if (shifter->secondary_transition) {
        state->button_banks[SHIFTER_TRANSITION_BUTTON_BANK] |= SHIFTER_TRANSITION_BUTTON_1;
    }
}

void fanatec_input_apply_thermal_limit(fanatec_input_state *state, bool active) {
    state->status_flags = (uint8_t)((state->status_flags & (uint8_t)~STATUS_THERMAL_EFFECT_LIMIT) |
                                    (active ? STATUS_THERMAL_EFFECT_LIMIT : 0));
}

void fanatec_input_apply_pedal_status(fanatec_input_state *state, bool legacy, bool auxiliary,
                                      bool handshake, bool resistance, bool calibration) {
    /** @brief Pedal transport status bit masks. */
    enum {
        PEDAL_LEGACY = 1u << 1,      /**< Legacy pedal transport status bit. */
        PEDAL_AUXILIARY = 1u << 2,   /**< Auxiliary pedal status bit. */
        PEDAL_HANDSHAKE = 1u << 3,   /**< Pedal startup handshake status bit. */
        PEDAL_RESISTANCE = 1u << 4,  /**< Pedal resistance status bit. */
        PEDAL_CALIBRATION = 1u << 5, /**< Pedal calibration status bit. */
        PEDAL_MASK = PEDAL_LEGACY | PEDAL_AUXILIARY | PEDAL_HANDSHAKE | PEDAL_RESISTANCE |
                     PEDAL_CALIBRATION, /**< Mask covering all pedal transport status bits. */
    };
    uint8_t status = (legacy ? PEDAL_LEGACY : 0) | (auxiliary ? PEDAL_AUXILIARY : 0) |
                     (handshake ? PEDAL_HANDSHAKE : 0) | (resistance ? PEDAL_RESISTANCE : 0) |
                     (calibration ? PEDAL_CALIBRATION : 0);
    state->status_flags = (uint8_t)((state->status_flags & (uint8_t)~PEDAL_MASK) | status);
}

void fanatec_input_apply_wheel_calibration(fanatec_input_state *state, bool available) {
    state->status_flags =
        (uint8_t)((state->status_flags & (uint8_t)~STATUS_WHEEL_CALIBRATION_AVAILABLE) |
                  (available ? STATUS_WHEEL_CALIBRATION_AVAILABLE : 0));
}

void fanatec_input_apply_wheel_input_capability(fanatec_input_state *state, bool available) {
    state->status_flags =
        (uint8_t)((state->status_flags & (uint8_t)~STATUS_WHEEL_INPUT_CAPABILITY) |
                  (available ? STATUS_WHEEL_INPUT_CAPABILITY : 0));
}

void fanatec_input_apply_wheel_axis_overrides(fanatec_input_state *state,
                                              const WheelAxisOverrides *overrides) {
    const WheelAxisOverride *pedal_overrides[FANATEC_INPUT_PEDAL_AXES] = {
        &overrides->axis_5,
        &overrides->axis_6,
        &overrides->axis_7,
    };
    for (uint8_t axis = 0; axis < FANATEC_INPUT_PEDAL_AXES; axis++) {
        if (pedal_overrides[axis]->enabled) {
            uint16_t value = (uint16_t)pedal_overrides[axis]->value * 0x0101u;
            if (value < state->pedals[axis]) {
                state->pedals[axis] = value;
            }
        }
    }
    if (overrides->auxiliary.enabled && overrides->auxiliary.value < state->auxiliary_pedal) {
        state->auxiliary_pedal = overrides->auxiliary.value;
    }
}

void fanatec_input_apply_bite_point_update(fanatec_input_state *state, uint8_t percent) {
    state->bite_point_percent = percent;
    state->bite_point_update = true;
}

/**
 * @brief Encodes the shared Fanatec input payload.
 *
 * Writes every logical input field and the fixed button usage identifiers without a report ID.
 *
 * @param[out] report Buffer that receives the encoded payload.
 * @param[in] state Logical Fanatec input values.
 */
static void encode_payload(uint8_t report[FANATEC_INPUT_COMPATIBILITY_REPORT_SIZE],
                           const fanatec_input_state *state) {
    size_t pedal;

    memcpy(report + BUTTONS_OFFSET, state->button_banks, sizeof(state->button_banks));
    memcpy(report + ROTARY_OFFSET, state->rotary, sizeof(state->rotary));
    memcpy(report + ACCESSORY_OFFSET, state->accessory, sizeof(state->accessory));
    report[TRANSFER_CODE_OFFSET] = state->transfer_code;
    write_u16(report + STEERING_OFFSET, state->steering);
    for (pedal = 0; pedal < FANATEC_INPUT_PEDAL_AXES; ++pedal) {
        write_u16(report + PEDALS_OFFSET + pedal * 2, state->pedals[pedal]);
    }
    memcpy(report + CLUTCH_PADDLES_OFFSET, state->clutch_paddles, sizeof(state->clutch_paddles));
    report[AUXILIARY_PEDAL_OFFSET] = state->auxiliary_pedal;
    report[ENCODER_OFFSET] = (uint8_t)state->encoder_position;
    report[STATUS_OFFSET] = state->status_flags;
    report[MODE_OFFSET] = state->wheel_mode;
    report[AXIS_LIMIT_OFFSET] = state->axis_limit;
    report[USAGE_PAGE_OFFSET] = BUTTON_USAGE_PAGE;
    report[USAGE_OFFSET] = BUTTON_USAGE;
}

bool fanatec_input_encode(uint8_t report[FANATEC_INPUT_REPORT_SIZE],
                          const fanatec_input_state *state) {
    if (report == NULL || state == NULL) {
        return false;
    }

    report[0] = FANATEC_INPUT_REPORT_ID;
    encode_payload(report + 1, state);
    if (state->bite_point_update) {
        report[BITE_POINT_UPDATE_OFFSET] = BITE_POINT_UPDATE_MARKER;
        report[BITE_POINT_UPDATE_OFFSET + 1] = BITE_POINT_UPDATE_TYPE;
        report[BITE_POINT_UPDATE_OFFSET + 2] = state->bite_point_percent;
        report[BITE_POINT_UPDATE_OFFSET + 3] = 0;
    }
    return true;
}

bool fanatec_input_compatibility_encode(uint8_t report[FANATEC_INPUT_COMPATIBILITY_REPORT_SIZE],
                                        const fanatec_input_state *state) {
    if (report == NULL || state == NULL) {
        return false;
    }

    encode_payload(report, state);
    report[FANATEC_INPUT_COMPATIBILITY_REPORT_SIZE - 1] = 0;
    return true;
}
