#include "usb/fanatec_input.h"

#include <stddef.h>
#include <string.h>

enum {
    BUTTONS_OFFSET = 1,
    ROTARY_OFFSET = 6,
    ACCESSORY_OFFSET = 11,
    TRANSFER_CODE_OFFSET = 16,
    STEERING_OFFSET = 17,
    PEDALS_OFFSET = 19,
    CLUTCH_PADDLES_OFFSET = 25,
    AUXILIARY_PEDAL_OFFSET = 27,
    ENCODER_OFFSET = 28,
    STATUS_OFFSET = 29,
    MODE_OFFSET = 30,
    AXIS_LIMIT_OFFSET = 31,
    USAGE_PAGE_OFFSET = 32,
    USAGE_OFFSET = 33,
    BUTTON_USAGE_PAGE = 9,
    BUTTON_USAGE = 3,
    SHIFTER_TRANSITION_BUTTON_BANK = 1,
    SHIFTER_GEAR_BUTTON_BANK = 2,
    SHIFTER_SEQUENTIAL_BUTTON_BANK = 4,
    SHIFTER_TRANSITION_BUTTON_0 = 1 << 0,
    SHIFTER_TRANSITION_BUTTON_1 = 1 << 1,
};

static void write_u16(uint8_t *destination, uint16_t value) {
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8);
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

bool fanatec_input_encode(uint8_t report[FANATEC_INPUT_REPORT_SIZE],
                          const fanatec_input_state *state) {
    size_t pedal;

    if (report == NULL || state == NULL) {
        return false;
    }

    memset(report, 0, FANATEC_INPUT_REPORT_SIZE);
    report[0] = FANATEC_INPUT_REPORT_ID;
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
    return true;
}
