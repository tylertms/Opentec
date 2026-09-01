#include "usb/playstation_input.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/** @brief Internal report layout values and wheel modes used by PlayStation input mapping. */
enum {
    PLAYSTATION_INPUT_REPORT_ID = 1,                /**< PlayStation input report identifier. */
    PLAYSTATION_INPUT_NEUTRAL_AXIS = 0x80,          /**< Neutral controller-axis value. */
    PLAYSTATION_INPUT_AXES_OFFSET = 1,              /**< Controller-axis field offset. */
    PLAYSTATION_INPUT_CONTROLS_OFFSET = 5,          /**< Hat and button field offset. */
    PLAYSTATION_INPUT_STEERING_OFFSET = 0x2b,       /**< Steering field offset. */
    PLAYSTATION_INPUT_PEDALS_OFFSET = 0x2d,         /**< Pedal field offset. */
    PLAYSTATION_INPUT_WHEEL_HAT_OFFSET = 0x33,      /**< Attached-wheel hat field offset. */
    PLAYSTATION_INPUT_AUXILIARY_AXIS_OFFSET = 0x34, /**< Auxiliary-axis field offset. */
    PLAYSTATION_INPUT_BUTTON_MASK = 0x3fff,         /**< Encoded PlayStation button mask. */
    PLAYSTATION_INPUT_VENDOR_BUTTON_MASK = 0x3f,    /**< Encoded vendor-button mask. */
    PLAYSTATION_INPUT_HAT_NEUTRAL = 8,              /**< Neutral PlayStation hat value. */
    PLAYSTATION_WHEEL_MODE_01 = 0x01,               /**< Wheel mode 0x01. */
    PLAYSTATION_WHEEL_MODE_02 = 0x02,               /**< Wheel mode 0x02. */
    PLAYSTATION_WHEEL_MODE_03 = 0x03,               /**< Wheel mode 0x03. */
    PLAYSTATION_WHEEL_MODE_04 = 0x04,               /**< Wheel mode 0x04. */
    PLAYSTATION_WHEEL_MODE_06 = 0x06,               /**< Wheel mode 0x06. */
    PLAYSTATION_WHEEL_MODE_09 = 0x09,               /**< Wheel mode 0x09. */
    PLAYSTATION_WHEEL_MODE_0A = 0x0a,               /**< Wheel mode 0x0A. */
    PLAYSTATION_WHEEL_MODE_0B = 0x0b,               /**< Wheel mode 0x0B. */
    PLAYSTATION_WHEEL_MODE_0C = 0x0c,               /**< Wheel mode 0x0C. */
    PLAYSTATION_WHEEL_MODE_0E = 0x0e,               /**< Wheel mode 0x0E. */
    PLAYSTATION_WHEEL_MODE_0F = 0x0f,               /**< Wheel mode 0x0F. */
    PLAYSTATION_WHEEL_MODE_10 = 0x10,               /**< Wheel mode 0x10. */
    PLAYSTATION_WHEEL_MODE_11 = 0x11,               /**< Wheel mode 0x11. */
    PLAYSTATION_WHEEL_MODE_13 = 0x13,               /**< Wheel mode 0x13. */
    PLAYSTATION_WHEEL_MODE_14 = 0x14,               /**< Wheel mode 0x14. */
    PLAYSTATION_WHEEL_MODE_15 = 0x15,               /**< Wheel mode 0x15. */
    PLAYSTATION_WHEEL_MODE_16 = 0x16,               /**< Wheel mode 0x16. */
    PLAYSTATION_WHEEL_MODE_17 = 0x17,               /**< Wheel mode 0x17. */
    PLAYSTATION_WHEEL_MODE_1C = 0x1c,               /**< Wheel mode 0x1C. */
    PLAYSTATION_WHEEL_MODE_1D = 0x1d,               /**< Wheel mode 0x1D. */
    PLAYSTATION_DUAL_CLUTCH_PADDLE_MODE = 4,        /**< Dual-clutch paddle mode value. */
    PLAYSTATION_SYSTEM_BUTTON_HOLD_MS = 3000,       /**< Required system-button hold duration. */
    PLAYSTATION_REPORT_BUTTON_COUNT = 13,           /**< Number of PlayStation report buttons. */
};

/** @brief Maps encoded wheel direction combinations to PlayStation hat values. */
static const uint8_t playstation_hat_map[16] = {8, 2, 6, 8, 4, 3, 5, 0, 0, 1, 7, 0, 8, 0, 2, 5};

/**
 * @brief Reads one source button bit.
 *
 * Extracts the requested bit as a Boolean value.
 *
 * @param[in] value Source button field.
 * @param[in] bit Zero-based source bit.
 * @return True when the source bit is set; otherwise false.
 */
static bool button_bit(uint16_t value, uint8_t bit) { return ((value >> bit) & 1u) != 0; }

/**
 * @brief Assigns one PlayStation report button.
 *
 * Sets or clears the requested logical report button without changing the other buttons.
 *
 * @param[in,out] buttons Logical PlayStation report buttons.
 * @param[in] button Zero-based report button.
 * @param[in] active True to set the button; false to clear it.
 */
static void assign_button(uint16_t *buttons, uint8_t button, bool active) {
    uint16_t mask = (uint16_t)(1u << button);
    *buttons = (uint16_t)((*buttons & (uint16_t)~mask) | (active ? mask : 0));
}

/**
 * @brief Merges one active source into a PlayStation report button.
 *
 * Sets the requested logical report button when the source is active and otherwise preserves it.
 *
 * @param[in,out] buttons Logical PlayStation report buttons.
 * @param[in] button Zero-based report button.
 * @param[in] active True to set the button.
 */
static void merge_button(uint16_t *buttons, uint8_t button, bool active) {
    if (active) {
        *buttons |= (uint16_t)(1u << button);
    }
}

/**
 * @brief Reports whether a wheel mode uses the attached adapter button policy.
 *
 * Selects the four modes that can replace or merge wheel controls with adapter buttons.
 *
 * @param[in] wheel_mode Attached-wheel protocol mode.
 * @return True when adapter button mapping applies; otherwise false.
 */
static bool uses_adapter_buttons(uint8_t wheel_mode) {
    return wheel_mode == PLAYSTATION_WHEEL_MODE_04 || wheel_mode == PLAYSTATION_WHEEL_MODE_06 ||
           wheel_mode == PLAYSTATION_WHEEL_MODE_0C || wheel_mode == PLAYSTATION_WHEEL_MODE_15;
}

/**
 * @brief Reports whether a monotonic deadline has elapsed.
 *
 * Uses signed modular subtraction so millisecond-counter wraparound does not extend a short hold.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] deadline_ms Target monotonic time in milliseconds.
 * @return True at or after the deadline; otherwise false.
 */
static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

/**
 * @brief Applies the direct attached-wheel button policy.
 *
 * Maps directional and secondary controls into the thirteen PlayStation report buttons before
 * adapter and auxiliary inputs are merged.
 *
 * @param[in] input Current attached-wheel button sources and mode.
 * @param[out] buttons Logical PlayStation report buttons.
 */
static void map_wheel_buttons(const UsbPlaystationButtonInput *input, uint16_t *buttons) {
    uint8_t mode = input->wheel_mode;
    uint8_t directional = input->directional_buttons;
    uint16_t secondary = input->secondary_buttons;

    if (mode != PLAYSTATION_WHEEL_MODE_0E) {
        assign_button(buttons, 0, button_bit(directional, 5));
    }
    bool alternate_legacy = mode == PLAYSTATION_WHEEL_MODE_0F || mode == PLAYSTATION_WHEEL_MODE_17;
    assign_button(buttons, 1,
                  button_bit(directional, 4) || button_bit(secondary, alternate_legacy ? 6 : 10));
    if (mode == PLAYSTATION_WHEEL_MODE_0A || mode == PLAYSTATION_WHEEL_MODE_10) {
        merge_button(buttons, 1, button_bit(secondary, 9));
    }

    if (mode == PLAYSTATION_WHEEL_MODE_0E) {
        assign_button(buttons, 0, button_bit(secondary, 1));
        assign_button(buttons, 1, button_bit(directional, 5));
        assign_button(buttons, 2, button_bit(directional, 7));
        assign_button(buttons, 3, button_bit(secondary, 5));
        assign_button(buttons, 4, button_bit(secondary, 3));
        assign_button(buttons, 5, button_bit(secondary, 0));
        assign_button(buttons, 6, button_bit(secondary, 4));
        assign_button(buttons, 7, button_bit(directional, 6));
        assign_button(buttons, 8, button_bit(secondary, 6));
        assign_button(buttons, 9, button_bit(secondary, 7));
        assign_button(buttons, 10, button_bit(input->auxiliary_history, 5));
        assign_button(buttons, 11, button_bit(input->auxiliary_history, 6));
        return;
    }

    if (input->adapter_mode != 0) {
        assign_button(buttons, 2, button_bit(directional, 6));
        assign_button(buttons, 11, button_bit(secondary, 2));
    }
    assign_button(buttons, 3, button_bit(directional, 7));
    assign_button(buttons, 4, button_bit(secondary, 3));
    assign_button(buttons, 5, button_bit(secondary, 0));
    assign_button(buttons, 10, button_bit(secondary, 5));
    if (alternate_legacy) {
        assign_button(buttons, 6, button_bit(secondary, 4));
        assign_button(buttons, 7, button_bit(secondary, 1));
        assign_button(buttons, 8, button_bit(secondary, 10));
        assign_button(buttons, 9, button_bit(secondary, 8));
    } else {
        assign_button(buttons, 8, button_bit(secondary, 6));
        assign_button(buttons, 9, button_bit(secondary, 7));
    }

    if (mode == PLAYSTATION_WHEEL_MODE_09 || mode == PLAYSTATION_WHEEL_MODE_0B ||
        mode == PLAYSTATION_WHEEL_MODE_1D) {
        assign_button(buttons, 6, false);
        assign_button(buttons, 7, false);
    } else if (mode == PLAYSTATION_WHEEL_MODE_0A || mode == PLAYSTATION_WHEEL_MODE_10 ||
               mode == PLAYSTATION_WHEEL_MODE_13 || mode == PLAYSTATION_WHEEL_MODE_14) {
        assign_button(buttons, 6, button_bit(secondary, 4));
        assign_button(buttons, 7, button_bit(secondary, 1));
    } else if (mode == PLAYSTATION_WHEEL_MODE_11) {
        assign_button(buttons, 6, button_bit(secondary, 4));
        assign_button(buttons, 7, button_bit(secondary, 2));
        assign_button(buttons, 11, button_bit(secondary, 1));
    } else {
        assign_button(buttons, 6, button_bit(secondary, 4));
        assign_button(buttons, 7, button_bit(secondary, 1));
    }
}

/**
 * @brief Applies attached-adapter button overrides and merges.
 *
 * Uses the adapter's two supported layouts to replace selected wheel controls and merge selected
 * adapter controls into the report button field.
 *
 * @param[in] input Current attached-wheel and adapter button sources.
 * @param[in,out] buttons Logical PlayStation report buttons.
 */
static void map_adapter_buttons(const UsbPlaystationButtonInput *input, uint16_t *buttons) {
    if (!uses_adapter_buttons(input->wheel_mode) || !input->adapter_connected) {
        return;
    }
    uint8_t first = input->adapter_buttons[0];
    uint8_t second = input->adapter_buttons[1];
    uint8_t third = input->adapter_buttons[2];

    if (input->adapter_mode == 0) {
        merge_button(buttons, 0, button_bit(second, 4));
        merge_button(buttons, 1,
                     button_bit(third, 2) || button_bit(second, 3) || button_bit(third, 3));
        assign_button(buttons, 2, button_bit(second, 2));
        merge_button(buttons, 3, button_bit(second, 1));
        merge_button(buttons, 6, button_bit(first, 4));
        merge_button(buttons, 7, button_bit(first, 6));
        merge_button(buttons, 8, button_bit(second, 0));
        merge_button(buttons, 9, button_bit(second, 5));
        merge_button(buttons, 10, button_bit(first, 5));
    } else if (input->adapter_mode == 1) {
        assign_button(buttons, 0, button_bit(first, 6));
        assign_button(buttons, 1,
                      button_bit(third, 2) || button_bit(second, 1) || button_bit(third, 3));
        merge_button(buttons, 2, button_bit(second, 2));
        merge_button(buttons, 3, button_bit(first, 4));
        merge_button(buttons, 6, button_bit(first, 5));
        merge_button(buttons, 7, button_bit(second, 3));
        merge_button(buttons, 8, button_bit(first, 7));
        merge_button(buttons, 9, button_bit(second, 0));
        merge_button(buttons, 10, button_bit(third, 1));
        merge_button(buttons, 11, button_bit(second, 6));
    }
}

/**
 * @brief Applies auxiliary and mode-specific system buttons.
 *
 * Maps auxiliary button fields, selects the mode-specific system-button source, enforces
 * suppression, and handles the three-second hold required by mode 0x0C without an adapter.
 *
 * @param[in,out] mapper Retained system-button hold timing.
 * @param[in] input Current attached-wheel, adapter, and auxiliary sources.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in,out] buttons Logical PlayStation report buttons.
 */
static void map_auxiliary_buttons(UsbPlaystationInputMapper *mapper,
                                  const UsbPlaystationButtonInput *input, uint32_t now_ms,
                                  uint16_t *buttons) {
    uint8_t mode = input->wheel_mode;
    uint16_t secondary = input->secondary_buttons;
    if (input->axis_modes[0] == 2 || input->axis_modes[1] == 2) {
        merge_button(buttons, 4, button_bit(input->auxiliary_buttons[1], 0));
        merge_button(buttons, 5, button_bit(input->auxiliary_buttons[0], 0));
    }

    assign_button(buttons, 12, button_bit(secondary, 9));
    if (mode == PLAYSTATION_WHEEL_MODE_0A || mode == PLAYSTATION_WHEEL_MODE_10 ||
        mode == PLAYSTATION_WHEEL_MODE_0E) {
        assign_button(buttons, 12, button_bit(secondary, 8));
    } else if (mode == PLAYSTATION_WHEEL_MODE_0F || mode == PLAYSTATION_WHEEL_MODE_17) {
        assign_button(buttons, 12, button_bit(secondary, 7));
    } else if (mode >= PLAYSTATION_WHEEL_MODE_01 && mode <= PLAYSTATION_WHEEL_MODE_03) {
        assign_button(buttons, 12, button_bit(secondary, 11));
        merge_button(buttons, 1, button_bit(secondary, 9));
    } else if (mode == PLAYSTATION_WHEEL_MODE_13 || mode == PLAYSTATION_WHEEL_MODE_14 ||
               mode == PLAYSTATION_WHEEL_MODE_16 || mode == PLAYSTATION_WHEEL_MODE_1C) {
        assign_button(buttons, 12, button_bit(secondary, 8));
        merge_button(buttons, 1, button_bit(secondary, 9));
        if (mode == PLAYSTATION_WHEEL_MODE_1C) {
            merge_button(buttons, 1, button_bit(input->extended_buttons, 1));
        }
    } else if (uses_adapter_buttons(mode)) {
        if (input->adapter_connected) {
            if (input->adapter_mode == 1) {
                assign_button(buttons, 12, button_bit(input->adapter_buttons[1], 4));
            } else if (input->adapter_mode == 0) {
                assign_button(buttons, 12, button_bit(input->adapter_buttons[0], 7));
            }
        } else if (mode == PLAYSTATION_WHEEL_MODE_0C) {
            bool pressed = button_bit(secondary, 7);
            if (!mapper->system_button_hold_active && pressed) {
                mapper->system_button_deadline_ms = now_ms + PLAYSTATION_SYSTEM_BUTTON_HOLD_MS;
                mapper->system_button_hold_active = true;
            } else if (mapper->system_button_hold_active && !pressed) {
                mapper->system_button_hold_active = false;
            } else if (mapper->system_button_hold_active &&
                       deadline_reached(now_ms, mapper->system_button_deadline_ms)) {
                assign_button(buttons, 12, true);
            }
        }
    }

    bool adapter_owns_button =
        (mode == PLAYSTATION_WHEEL_MODE_04 || mode == PLAYSTATION_WHEEL_MODE_06 ||
         mode == PLAYSTATION_WHEEL_MODE_15) &&
        input->adapter_connected;
    if (mode != PLAYSTATION_WHEEL_MODE_0F && mode != PLAYSTATION_WHEEL_MODE_17 &&
        mode != PLAYSTATION_WHEEL_MODE_1C && mode != PLAYSTATION_WHEEL_MODE_0C &&
        !adapter_owns_button) {
        merge_button(buttons, 1, button_bit(secondary, 10));
    }
    if (input->system_button_suppressed || (secondary & 0x2000u) != 0) {
        assign_button(buttons, 12, false);
    }
}

void usb_playstation_input_mapper_init(UsbPlaystationInputMapper *mapper) {
    *mapper = (UsbPlaystationInputMapper){0};
}

/**
 * @brief Reports whether a wheel mode supplies one PlayStation clutch axis.
 *
 * Selects the four wheel modes that use the first clutch axis when the attached wheel advertises
 * it and center the second clutch axis.
 *
 * @param[in] wheel_mode Attached-wheel protocol mode.
 * @return True when the mode supplies one clutch axis; otherwise false.
 */
static bool uses_single_clutch_axis(uint8_t wheel_mode) {
    return wheel_mode == PLAYSTATION_WHEEL_MODE_09 || wheel_mode == PLAYSTATION_WHEEL_MODE_0B ||
           wheel_mode == PLAYSTATION_WHEEL_MODE_1C || wheel_mode == PLAYSTATION_WHEEL_MODE_1D;
}

/**
 * @brief Reports whether a wheel mode can source clutch axes from an adapter.
 *
 * Selects the four modes whose mapping can use attached-adapter clutch axes.
 *
 * @param[in] wheel_mode Attached-wheel protocol mode.
 * @return True when the mode can source adapter clutch axes; otherwise false.
 */
static bool uses_adapter_clutch_axes(uint8_t wheel_mode) {
    return wheel_mode == PLAYSTATION_WHEEL_MODE_04 || wheel_mode == PLAYSTATION_WHEEL_MODE_06 ||
           wheel_mode == PLAYSTATION_WHEEL_MODE_0C || wheel_mode == PLAYSTATION_WHEEL_MODE_15;
}

/**
 * @brief Reports whether a wheel mode supplies two PlayStation clutch axes.
 *
 * Selects the protocol modes that map both attached-wheel paddle values around the controller-axis
 * center.
 *
 * @param[in] wheel_mode Attached-wheel protocol mode.
 * @return True when the mode supplies two clutch axes; otherwise false.
 */
static bool uses_dual_clutch_axes(uint8_t wheel_mode) {
    return (wheel_mode >= PLAYSTATION_WHEEL_MODE_01 && wheel_mode <= PLAYSTATION_WHEEL_MODE_03) ||
           wheel_mode == PLAYSTATION_WHEEL_MODE_0A || wheel_mode == PLAYSTATION_WHEEL_MODE_0E ||
           wheel_mode == PLAYSTATION_WHEEL_MODE_0F || wheel_mode == PLAYSTATION_WHEEL_MODE_13 ||
           wheel_mode == PLAYSTATION_WHEEL_MODE_14 || wheel_mode == PLAYSTATION_WHEEL_MODE_16 ||
           wheel_mode == PLAYSTATION_WHEEL_MODE_17;
}

/**
 * @brief Centers the two PlayStation clutch axes.
 *
 * Writes the controller-axis midpoint to both output channels.
 *
 * @param[out] axes Two clutch-axis output bytes.
 */
static void center_clutch_axes(uint8_t axes[2]) {
    axes[0] = PLAYSTATION_INPUT_NEUTRAL_AXIS;
    axes[1] = PLAYSTATION_INPUT_NEUTRAL_AXIS;
}

/**
 * @brief Maps two attached-wheel paddle values around the controller-axis center.
 *
 * Inverts the first paddle below center and adds the second paddle above center with eight-bit
 * wraparound matching the USB field width.
 *
 * @param[out] axes Two clutch-axis output bytes.
 * @param[in] wheel_axes Two normalized attached-wheel paddle values.
 */
static void map_dual_clutch_axes(uint8_t axes[2], const uint8_t wheel_axes[2]) {
    axes[0] = (uint8_t)(0x7fu - wheel_axes[0]);
    axes[1] = (uint8_t)(PLAYSTATION_INPUT_NEUTRAL_AXIS + wheel_axes[1]);
}

uint8_t usb_playstation_input_map_hat(uint8_t directional_buttons) {
    uint8_t index =
        (uint8_t)(((directional_buttons & 0x01u) << 3) | ((directional_buttons >> 1) & 0x04u) |
                  (directional_buttons & 0x02u) | ((directional_buttons >> 2) & 0x01u));
    return playstation_hat_map[index];
}

void usb_playstation_input_map_clutch(uint8_t axes[2], const UsbPlaystationClutchInput *input) {
    if (axes == 0) {
        return;
    }
    center_clutch_axes(axes);
    if (input == 0) {
        return;
    }
    if (uses_single_clutch_axis(input->wheel_mode)) {
        if (input->wheel_axis_enabled) {
            axes[0] = (uint8_t)(0x7fu - input->wheel_axes[0]);
        }
        return;
    }
    if (uses_adapter_clutch_axes(input->wheel_mode)) {
        if (input->paddle_mode == PLAYSTATION_DUAL_CLUTCH_PADDLE_MODE) {
            map_dual_clutch_axes(axes, input->wheel_axes);
        } else if (input->adapter_connected) {
            axes[0] = input->adapter_axes[1];
            axes[1] = (uint8_t)~input->adapter_axes[0];
        }
        return;
    }
    if (uses_dual_clutch_axes(input->wheel_mode)) {
        map_dual_clutch_axes(axes, input->wheel_axes);
    }
}

bool usb_playstation_input_map_buttons(UsbPlaystationInputMapper *mapper,
                                       const UsbPlaystationButtonInput *input, uint32_t now_ms,
                                       UsbPlaystationInputState *state) {
    if (mapper == 0 || input == 0 || state == 0) {
        return false;
    }

    state->hat = input->hat_suppressed || (input->secondary_buttons & 0x2000u) != 0
                     ? PLAYSTATION_INPUT_HAT_NEUTRAL
                     : usb_playstation_input_map_hat(input->directional_buttons);
    state->buttons = 0;
    state->vendor_buttons = 0;
    map_wheel_buttons(input, &state->buttons);
    map_adapter_buttons(input, &state->buttons);
    map_auxiliary_buttons(mapper, input, now_ms, &state->buttons);
    state->buttons &= (uint16_t)((1u << PLAYSTATION_REPORT_BUTTON_COUNT) - 1u);
    return true;
}

/**
 * @brief Writes a little-endian sixteen-bit PlayStation axis value.
 *
 * Stores the least-significant axis byte first in the two-byte report field.
 *
 * @param[out] destination First byte of the encoded axis.
 * @param[in] value Axis value to encode.
 */
static void write_axis(uint8_t *destination, uint16_t value) {
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8);
}

/**
 * @brief Rotates the attached-wheel hat into its vendor-field orientation.
 *
 * Moves bit zero to bit seven and shifts the remaining bits down by one position.
 *
 * @param[in] value Attached-wheel hat byte.
 * @return Rotated vendor-field value.
 */
static uint8_t rotate_wheel_hat(uint8_t value) { return (uint8_t)((value << 7) | (value >> 1)); }

bool usb_playstation_input_encode(uint8_t report[USB_PLAYSTATION_INPUT_REPORT_SIZE],
                                  const UsbPlaystationInputState *state) {
    if (report == 0 || state == 0 || state->hat > PLAYSTATION_INPUT_HAT_NEUTRAL) {
        return false;
    }

    memset(report, 0, USB_PLAYSTATION_INPUT_REPORT_SIZE);
    report[0] = PLAYSTATION_INPUT_REPORT_ID;
    report[PLAYSTATION_INPUT_AXES_OFFSET] = PLAYSTATION_INPUT_NEUTRAL_AXIS;
    report[PLAYSTATION_INPUT_AXES_OFFSET + 1] = PLAYSTATION_INPUT_NEUTRAL_AXIS;
    report[PLAYSTATION_INPUT_AXES_OFFSET + 2] = state->clutch_axes[0];
    report[PLAYSTATION_INPUT_AXES_OFFSET + 3] = state->clutch_axes[1];

    uint32_t controls =
        (uint32_t)state->hat | ((uint32_t)(state->buttons & PLAYSTATION_INPUT_BUTTON_MASK) << 4) |
        ((uint32_t)(state->vendor_buttons & PLAYSTATION_INPUT_VENDOR_BUTTON_MASK) << 18);
    report[PLAYSTATION_INPUT_CONTROLS_OFFSET] = (uint8_t)controls;
    report[PLAYSTATION_INPUT_CONTROLS_OFFSET + 1] = (uint8_t)(controls >> 8);
    report[PLAYSTATION_INPUT_CONTROLS_OFFSET + 2] = (uint8_t)(controls >> 16) & 1u;

    write_axis(report + PLAYSTATION_INPUT_STEERING_OFFSET, state->steering);
    for (uint8_t axis = 0; axis < USB_PLAYSTATION_INPUT_PEDAL_COUNT; axis++) {
        write_axis(report + PLAYSTATION_INPUT_PEDALS_OFFSET + axis * 2, state->pedals[axis]);
    }
    report[PLAYSTATION_INPUT_WHEEL_HAT_OFFSET] = rotate_wheel_hat(state->wheel_hat);
    write_axis(report + PLAYSTATION_INPUT_AUXILIARY_AXIS_OFFSET, state->auxiliary_axis);
    return true;
}
