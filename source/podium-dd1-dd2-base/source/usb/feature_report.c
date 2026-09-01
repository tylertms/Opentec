#include "usb/feature_report.h"

#include <string.h>

#include "usb/tuning_profile_report.h"

/** @brief Native feature report identifiers and rotary presentation modes. */
enum {
    REPORT_31 = 0x31,            /**< Native status feature report identifier. */
    REPORT_32 = 0x32,            /**< Native tuning-profile feature report identifier. */
    REPORT_33 = 0x33,            /**< Native rotary and button feature report identifier. */
    REPORT_36 = 0x36,            /**< Native tuning-menu feature report identifier. */
    ROTARY_EVENTS = 0,           /**< Rotary event presentation mode. */
    ROTARY_POSITION_CHANGES = 1, /**< Rotary position-change presentation mode. */
    ROTARY_POSITIONS = 2,        /**< Rotary constant-position presentation mode. */
};

/**
 * @brief Combines two shifter axis modes for feature report 31.
 *
 * Returns code one when either input selects code one, then code two when either input selects code
 * two, and zero when neither input selects an axis mode.
 *
 * @param[in] modes Primary and secondary shifter axis modes.
 * @return Combined axis-mode code.
 */
static uint8_t axis_mode(const uint8_t modes[2]) {
    if (modes[0] == 1 || modes[1] == 1) {
        return 1;
    }
    return modes[0] == 2 || modes[1] == 2 ? 2 : 0;
}

void usb_feature_report_31_encode(const UsbFeatureReport31State *state,
                                  uint8_t output[USB_DEVICE_REPORT_SIZE]) {
    memset(output, 0, USB_DEVICE_REPORT_SIZE);
    output[0] = REPORT_31;
    output[1] = 1;
    output[2] = (state->axis_modes[0] == 2 && state->axis_modes[1] == 2 ? 0x01 : 0) |
                (state->pedal_legacy ? 0x02 : 0) | (state->pedal_io_active ? 0x04 : 0) |
                (state->pedal_handshake_active ? 0x08 : 0) | (state->resistance_active ? 0x10 : 0) |
                (state->pedal_calibration_active ? 0x20 : 0) |
                (state->wheel_calibration_available ? 0x40 : 0) |
                (state->wheel_axis_report_enabled ? 0x80 : 0);
    output[3] = state->wheel_mode;
    output[4] = state->pedal_active;
    output[5] = state->auxiliary_profile;
    output[6] = axis_mode(state->axis_modes);
    output[7] = 3;
    output[8] = 9;
    output[9] = state->transfer_code;
    output[10] = state->rotary_mode;
    output[11] = state->adapter_connected;
    output[12] = (uint8_t)state->status;
    output[13] = (uint8_t)(state->status >> 8);
}

void usb_feature_report_32_encode(const TuningProfileBank *bank, bool dirty,
                                  uint8_t output[USB_DEVICE_REPORT_SIZE]) {
    memset(output, 0, USB_DEVICE_REPORT_SIZE);
    output[0] = REPORT_32;
    output[1] = (uint8_t)(bank->active_slot + 1u);
    output[2] = (uint8_t)(bank->selected_slot + 1u);
    output[3] = dirty;
    usb_tuning_profile_report_encode(&bank->slots[bank->active_slot], output + 5);
}

/**
 * @brief Encodes one multi-position selector value.
 *
 * Converts positions one through twelve to a single selection bit and applies the reference remap
 * permutation when requested. Unsupported positions produce zero.
 *
 * @param[in] position One-based multi-position selector position.
 * @param[in] remap True to apply the alternate selector bit mapping.
 * @return Encoded 12-bit selector value.
 */
static uint16_t selector(uint8_t position, bool remap) {
    if (position == 0 || position > 12) {
        return 0;
    }
    uint16_t value = (uint16_t)(1u << (position - 1u));
    return !remap ? value : value <= 8 ? (uint16_t)(value << 8) : (uint16_t)(value >> 4);
}

/**
 * @brief Packs one multi-position selector into feature report 33.
 *
 * Places the 12-bit selector in the channel-specific split fields while preserving neighboring
 * channel bits that share an output byte.
 *
 * @param[in,out] output Feature-report payload receiving the selector.
 * @param[in] channel Zero-based selector channel.
 * @param[in] value Encoded 12-bit selector value.
 */
static void encode_selector(uint8_t *output, uint8_t channel, uint16_t value) {
    if (channel == 0) {
        output[1] = (uint8_t)value;
        output[2] |= (uint8_t)((value >> 8) & 0x0f);
    } else if (channel == 1) {
        output[2] |= (uint8_t)(value << 4);
        output[3] = (uint8_t)(value >> 4);
    } else {
        output[6] |= (uint8_t)value;
        output[7] = (uint8_t)((value >> 8) & 0x0f);
    }
}

/**
 * @brief Selects a pulse flag from a signed motion direction.
 *
 * Returns the positive flag for forward motion, the negative flag for reverse motion, and zero for
 * no motion.
 *
 * @param[in] direction Signed motion direction.
 * @param[in] positive Flag representing positive motion.
 * @param[in] negative Flag representing negative motion.
 * @return Selected pulse flag.
 */
static uint8_t pulse_bit(int8_t direction, uint8_t positive, uint8_t negative) {
    return direction > 0 ? positive : direction < 0 ? negative : 0;
}

void usb_feature_report_33_encode(const UsbFeatureReport33State *state,
                                  uint8_t output[USB_DEVICE_REPORT_SIZE]) {
    memset(output, 0, USB_DEVICE_REPORT_SIZE);
    output[0] = REPORT_33;
    for (uint8_t channel = 0; channel < 3; channel++) {
        if (channel == 2 && !state->tertiary_active) {
            continue;
        }
        if (state->rotary_mode == ROTARY_EVENTS) {
            if (channel == 0) {
                output[1] = (uint8_t)state->events[channel];
            } else if (channel == 1) {
                output[2] = (uint8_t)(state->events[channel] << 4);
            } else {
                output[6] = (uint8_t)state->events[channel];
            }
        } else if (state->rotary_mode == ROTARY_POSITIONS ||
                   (state->rotary_mode == ROTARY_POSITION_CHANGES && state->events[channel] != 0)) {
            encode_selector(output, channel,
                            selector(state->positions[channel], state->remap_selectors));
        }
    }
    output[5] = pulse_bit(state->pulse_directions[0], 0x02, 0x01) |
                pulse_bit(state->pulse_directions[1], 0x10, 0x20) |
                pulse_bit(state->pulse_directions[2], 0x04, 0x08) |
                pulse_bit(state->pulse_directions[3], 0x80, 0x40);
    output[12] = state->extended_buttons;
    output[13] = state->auxiliary_buttons[0];
    output[14] = state->auxiliary_buttons[1];
    output[15] = state->auxiliary_buttons[2];
}

void usb_feature_report_36_encode(uint8_t page, uint8_t output[USB_DEVICE_REPORT_SIZE]) {
    memset(output, 0, USB_DEVICE_REPORT_SIZE);
    output[0] = REPORT_36;
    output[1] = 2;
    output[2] = page;
}
