#include "usb/fanatec_input.h"

#include <stddef.h>
#include <string.h>

enum {
    BUTTONS_OFFSET = 0,
    ROTARY_OFFSET = 5,
    ACCESSORY_OFFSET = 10,
    TRANSFER_CODE_OFFSET = 15,
    STEERING_OFFSET = 16,
    PEDALS_OFFSET = 18,
    CLUTCH_PADDLES_OFFSET = 24,
    AUXILIARY_PEDAL_OFFSET = 26,
    ENCODER_OFFSET = 27,
    STATUS_OFFSET = 28,
    MODE_OFFSET = 29,
    AXIS_LIMIT_OFFSET = 30,
    USAGE_PAGE_OFFSET = 31,
    USAGE_OFFSET = 32,
    BUTTON_USAGE_PAGE = 9,
    BUTTON_USAGE = 3,
    SHIFTER_TRANSITION_BUTTON_BANK = 1,
    SHIFTER_GEAR_BUTTON_BANK = 2,
    SHIFTER_SEQUENTIAL_BUTTON_BANK = 4,
    SHIFTER_TRANSITION_BUTTON_0 = 1 << 0,
    SHIFTER_TRANSITION_BUTTON_1 = 1 << 1,
    MULTI_POSITION_ENCODER_MODE = 0,
    MULTI_POSITION_PULSE_MODE = 1,
    MULTI_POSITION_CONSTANT_MODE = 2,
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

/**
 * @brief Applies attached-wheel controls to the Fanatec input fields.
 *
 * Always copies the first two rotary values and the transfer byte. When extended controls are
 * enabled, also copies the remaining rotary values, the first accessory byte, and the low nibble
 * of the final accessory byte.
 *
 * @param[in,out] state Input report state to update.
 * @param[in] controls Eight attached-wheel control bytes.
 * @param[in] include_extended True to include control bytes two through six.
 */
void fanatec_input_apply_wheel_controls(fanatec_input_state *state, const uint8_t controls[8],
                                        bool include_extended) {
    state->rotary[0] = controls[0];
    state->rotary[1] = controls[1];
    state->transfer_code = controls[7];
    if (!include_extended) {
        return;
    }
    state->rotary[2] = controls[2];
    state->rotary[3] = controls[3];
    state->rotary[4] = controls[4];
    state->accessory[0] = controls[5];
    state->accessory[4] = (uint8_t)((state->accessory[4] & 0xf0u) | (controls[6] & 0x0fu));
}

/**
 * @brief Applies the multi-position reporting mode to the Fanatec input state.
 *
 * Replaces bits four and five of the final accessory byte with the low two bits of the effective
 * mode.
 *
 * @param[in,out] state Input report state to update.
 * @param[in] mode Effective multi-position reporting mode.
 */
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
 * Places each channel in its twelve-bit report slot without changing bits outside that slot.
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

/**
 * @brief Applies multi-position rotary values to the Fanatec input state.
 *
 * Encoder mode emits direction events. Pulse mode emits a position while its transition event is
 * active. Constant mode continuously emits each active position. Other modes leave all rotary
 * fields clear.
 *
 * @param[in,out] state Input report state to update.
 * @param[in] mode Effective multi-position reporting mode.
 * @param[in] input Logical rotary channels and selector layout.
 */
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

/**
 * @brief Applies shifter input to the Fanatec button fields.
 *
 * Places the current H-pattern gear or sequential transition buttons according to the two shifter
 * port modes.
 *
 * @param[in,out] state Input report state to update.
 * @param[in] shifter Mode and transition state for both shifter ports.
 * @param[in] gear Current H-pattern gear bit, or neutral.
 */
void fanatec_input_apply_shifter(fanatec_input_state *state, const ShifterInputState *shifter,
                                 ShifterGear gear) {
    bool sequential_only = shifter->primary_mode == SHIFTER_INPUT_SEQUENTIAL &&
                           shifter->secondary_mode == SHIFTER_INPUT_SEQUENTIAL;
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
    report[ENCODER_OFFSET] = (uint8_t)state->encoder_delta;
    report[STATUS_OFFSET] = state->status_flags;
    report[MODE_OFFSET] = state->wheel_mode;
    report[AXIS_LIMIT_OFFSET] = state->axis_limit;
    report[USAGE_PAGE_OFFSET] = BUTTON_USAGE_PAGE;
    report[USAGE_OFFSET] = BUTTON_USAGE;
}

/**
 * @brief Encodes the native Fanatec input report.
 *
 * Writes report ID one followed by the complete thirty-three-byte Fanatec input payload.
 *
 * @param[out] report Buffer that receives the encoded report.
 * @param[in] state Logical Fanatec input values.
 * @return True when the report was encoded; otherwise false.
 */
bool fanatec_input_encode(uint8_t report[FANATEC_INPUT_REPORT_SIZE],
                          const fanatec_input_state *state) {
    if (report == NULL || state == NULL) {
        return false;
    }

    report[0] = FANATEC_INPUT_REPORT_ID;
    encode_payload(report + 1, state);
    return true;
}

/**
 * @brief Encodes the Fanatec compatibility input report.
 *
 * Writes the complete Fanatec input payload without a leading HID report ID.
 *
 * @param[out] report Buffer that receives the encoded report.
 * @param[in] state Logical Fanatec input values.
 * @return True when the report was encoded; otherwise false.
 */
bool fanatec_input_compatibility_encode(uint8_t report[FANATEC_INPUT_COMPATIBILITY_REPORT_SIZE],
                                        const fanatec_input_state *state) {
    if (report == NULL || state == NULL) {
        return false;
    }

    encode_payload(report, state);
    return true;
}
