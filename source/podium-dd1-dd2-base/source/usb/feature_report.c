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

/**
 * @brief Reports whether a wheel mode publishes rotary feature data.
 *
 * @param[in] mode Attached-wheel mode.
 * @return True when rotary feature data is available; otherwise false.
 */
bool usb_feature_report_33_supports_rotary(uint8_t mode) {
    return (mode >= 0x09 && mode <= 0x0b) || mode == 0x04 || mode == 0x1d || mode == 0x06 ||
           mode == 0x0c || mode == 0x0f || mode == 0x17 || mode == 0x0e || mode == 0x18 ||
           mode == 0x1b || mode == 0x1c;
}

static bool report_33_uses_tertiary_rotary(uint8_t mode) {
    return mode == 0x0f || mode == 0x17 || mode == 0x1c;
}

static uint8_t encode_low_request_extended_buttons(uint16_t buttons) {
    return (uint8_t)(((buttons >> 14) & 0x03u) | ((buttons << 1) & 0x04u) |
                     ((buttons >> 1) & 0x08u));
}

static uint8_t encode_high_request_extended_buttons(uint16_t buttons) {
    return (uint8_t)(((buttons >> 14) & 0x03u) | ((buttons >> 9) & 0x0cu));
}

static uint8_t encode_adapter_extended_buttons(const uint8_t buttons[3]) {
    return (uint8_t)(((buttons[1] >> 6) & 0x02u) | ((buttons[2] & 0x01u) << 2) |
                     ((buttons[2] >> 1) & 0x01u) | ((buttons[1] >> 3) & 0x08u));
}

static uint8_t encode_control_extended_buttons(uint8_t buttons) {
    return (uint8_t)((buttons & 0x03u) << 4);
}

static uint8_t encode_extra_extended_buttons(uint8_t buttons) {
    return (uint8_t)(((buttons & 0x01u) << 4) | ((buttons & 0x04u) << 3));
}

static void encode_report_33_buttons(const UsbFeatureReport33State *state, uint8_t *output) {
    uint8_t mode = state->wheel_mode;
    if (mode == 0x01 || mode == 0x0e) {
        uint8_t first = state->auxiliary_report[0];
        uint8_t second = state->auxiliary_report[1];
        uint8_t third = state->auxiliary_report[2];
        output[13] = (uint8_t)(((first >> 2) & 0x02u) | ((first << 1) & 0x04u) | (first & 0x01u) |
                               ((first << 1) & 0x08u) | ((third & 0x01u) << 4) |
                               ((first << 1) & 0x20u) | ((first >> 7) << 6));
        if (mode != 0x0e) {
            output[13] |= (uint8_t)((first << 2) & 0x80u);
            output[14] = (uint8_t)((first >> 6) & 0x01u);
        } else {
            output[12] = encode_control_extended_buttons(state->control_extended[0]);
        }
        output[14] |= (uint8_t)((third & 0x02u) | ((second & 0x01u) << 2) | (second & 0x08u) |
                                ((second << 3) & 0x10u) | ((second << 3) & 0x20u) |
                                ((third << 4) & 0x40u) | ((second << 3) & 0x80u));
        output[15] |= (uint8_t)(((second >> 4) & 0x02u) | (second >> 7) | ((second >> 4) & 0x04u) |
                                (third & 0x08u));
        return;
    }
    if (mode == 0x09 || mode == 0x0b || mode == 0x1b) {
        output[12] = encode_low_request_extended_buttons(state->secondary_buttons);
        return;
    }
    if (mode == 0x0a || mode == 0x0f || mode == 0x17) {
        output[12] = encode_high_request_extended_buttons(state->secondary_buttons) |
                     encode_control_extended_buttons(state->control_extended[0]);
        return;
    }
    if (mode == 0x12) {
        output[12] = encode_high_request_extended_buttons(state->secondary_buttons);
        return;
    }
    if (mode == 0x1c) {
        uint8_t first = state->auxiliary_report[0];
        uint8_t third = state->auxiliary_report[2];
        output[12] = encode_high_request_extended_buttons(state->secondary_buttons) |
                     encode_control_extended_buttons(state->control_extended[0]);
        output[13] |=
            (uint8_t)(((first << 1) & 0x20u) | ((first >> 1) & 0x40u) | ((first << 2) & 0x80u));
        output[14] |=
            (uint8_t)(((third >> 1) & 0x02u) | ((first >> 6) & 0x01u) | ((third << 3) & 0x40u));
        return;
    }
    if (mode == 0x04 || mode == 0x06 || mode == 0x1d || mode == 0x0c) {
        output[12] = state->adapter_connected
                         ? encode_adapter_extended_buttons(state->adapter_buttons)
                         : encode_high_request_extended_buttons(state->secondary_buttons);
        output[12] |= mode == 0x0c ? encode_control_extended_buttons(state->control_extended[0])
                                   : encode_extra_extended_buttons(state->control_extended[1]);
        return;
    }
    output[12] = encode_control_extended_buttons(state->control_extended[0]);
}

void usb_feature_report_33_encode(const UsbFeatureReport33State *state,
                                  uint8_t output[USB_DEVICE_REPORT_SIZE]) {
    memset(output, 0, USB_DEVICE_REPORT_SIZE);
    output[0] = REPORT_33;
    bool supports_rotary = usb_feature_report_33_supports_rotary(state->wheel_mode);
    if (supports_rotary) {
        for (uint8_t channel = 0; channel < 3; channel++) {
            if (channel == 2 && !report_33_uses_tertiary_rotary(state->wheel_mode)) {
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
                       (state->rotary_mode == ROTARY_POSITION_CHANGES &&
                        state->events[channel] != 0)) {
                encode_selector(output, channel,
                                selector(state->positions[channel], state->remap_selectors));
            }
        }
        output[5] = pulse_bit(state->pulse_directions[0], 0x02, 0x01) |
                    pulse_bit(state->pulse_directions[1], 0x10, 0x20) |
                    pulse_bit(state->pulse_directions[2], 0x04, 0x08);
        if (state->wheel_mode == 0x0e || state->wheel_mode == 0x1c) {
            output[5] |= pulse_bit(state->pulse_directions[3], 0x80, 0x40);
        }
        if (state->wheel_mode == 0x0e) {
            output[1] = (uint8_t)state->events[0];
            output[2] = (uint8_t)(state->events[1] << 4);
            output[6] |= (uint8_t)(state->events[2] << 4);
        } else if (state->wheel_mode == 0x1b) {
            output[13] |= pulse_bit(state->pulse_directions[3], 0x01, 0x02);
            output[14] |= state->pulse_input_direction < 0 ? 0x80u : 0;
            output[15] |= state->pulse_input_direction > 0 ? 0x01u : 0;
        }
    } else if (!state->profile_state_suppresses_pulse) {
        output[5] = pulse_bit(state->pulse_directions[0], 0x02, 0x01);
    }
    encode_report_33_buttons(state, output);
}

void usb_feature_report_36_encode(uint8_t page, uint8_t output[USB_DEVICE_REPORT_SIZE]) {
    memset(output, 0, USB_DEVICE_REPORT_SIZE);
    output[0] = REPORT_36;
    output[1] = 2;
    output[2] = page;
}
