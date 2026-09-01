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
    return true;
}
