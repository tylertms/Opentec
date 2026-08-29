#include "usb/playstation_input.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum {
    PLAYSTATION_INPUT_REPORT_ID = 1,
    PLAYSTATION_INPUT_NEUTRAL_AXIS = 0x80,
    PLAYSTATION_INPUT_AXES_OFFSET = 1,
    PLAYSTATION_INPUT_CONTROLS_OFFSET = 5,
    PLAYSTATION_INPUT_STEERING_OFFSET = 0x2b,
    PLAYSTATION_INPUT_PEDALS_OFFSET = 0x2d,
    PLAYSTATION_INPUT_WHEEL_HAT_OFFSET = 0x33,
    PLAYSTATION_INPUT_AUXILIARY_AXIS_OFFSET = 0x34,
    PLAYSTATION_INPUT_BUTTON_MASK = 0x3fff,
    PLAYSTATION_INPUT_VENDOR_BUTTON_MASK = 0x3f,
    PLAYSTATION_INPUT_HAT_NEUTRAL = 8,
};

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

/**
 * @brief Encodes the PlayStation 64-byte input report.
 *
 * Writes report ID one, the four controller axes, packed hat and button fields, and the extended
 * steering, pedal, wheel-hat, and auxiliary axes. Unused vendor bytes remain zero.
 *
 * @param[out] report Destination for the complete input report.
 * @param[in] state Logical PlayStation input values.
 * @return True when the report was encoded; otherwise false.
 */
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
    report[PLAYSTATION_INPUT_CONTROLS_OFFSET + 2] = (uint8_t)(controls >> 16);

    write_axis(report + PLAYSTATION_INPUT_STEERING_OFFSET, state->steering);
    for (uint8_t axis = 0; axis < USB_PLAYSTATION_INPUT_PEDAL_COUNT; axis++) {
        write_axis(report + PLAYSTATION_INPUT_PEDALS_OFFSET + axis * 2, state->pedals[axis]);
    }
    report[PLAYSTATION_INPUT_WHEEL_HAT_OFFSET] = rotate_wheel_hat(state->wheel_hat);
    write_axis(report + PLAYSTATION_INPUT_AUXILIARY_AXIS_OFFSET, state->auxiliary_axis);
    return true;
}
