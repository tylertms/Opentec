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
    PLAYSTATION_WHEEL_MODE_01 = 0x01,
    PLAYSTATION_WHEEL_MODE_02 = 0x02,
    PLAYSTATION_WHEEL_MODE_03 = 0x03,
    PLAYSTATION_WHEEL_MODE_04 = 0x04,
    PLAYSTATION_WHEEL_MODE_06 = 0x06,
    PLAYSTATION_WHEEL_MODE_09 = 0x09,
    PLAYSTATION_WHEEL_MODE_0A = 0x0a,
    PLAYSTATION_WHEEL_MODE_0B = 0x0b,
    PLAYSTATION_WHEEL_MODE_0C = 0x0c,
    PLAYSTATION_WHEEL_MODE_0E = 0x0e,
    PLAYSTATION_WHEEL_MODE_0F = 0x0f,
    PLAYSTATION_WHEEL_MODE_13 = 0x13,
    PLAYSTATION_WHEEL_MODE_14 = 0x14,
    PLAYSTATION_WHEEL_MODE_15 = 0x15,
    PLAYSTATION_WHEEL_MODE_16 = 0x16,
    PLAYSTATION_WHEEL_MODE_17 = 0x17,
    PLAYSTATION_WHEEL_MODE_1C = 0x1c,
    PLAYSTATION_WHEEL_MODE_1D = 0x1d,
    PLAYSTATION_DUAL_CLUTCH_PADDLE_MODE = 4,
};

static const uint8_t playstation_hat_map[16] = {8, 2, 6, 8, 4, 3, 5, 0, 0, 1, 7, 0, 8, 0, 2, 5};

/**
 * @brief Reports whether a wheel mode supplies one PlayStation clutch axis.
 *
 * Selects the four wheel modes that center the second clutch axis and suppress the first axis when
 * the attached wheel does not advertise its axis report.
 *
 * @param[in] wheel_mode Attached-wheel protocol mode.
 * @return True when the mode supplies one clutch axis.
 */
static bool uses_single_clutch_axis(uint8_t wheel_mode) {
    return wheel_mode == PLAYSTATION_WHEEL_MODE_09 || wheel_mode == PLAYSTATION_WHEEL_MODE_0B ||
           wheel_mode == PLAYSTATION_WHEEL_MODE_1C || wheel_mode == PLAYSTATION_WHEEL_MODE_1D;
}

/**
 * @brief Reports whether a wheel mode can source clutch axes from an adapter.
 *
 * Selects the four modes that use attached-adapter axes unless dual-clutch paddle mode is active.
 *
 * @param[in] wheel_mode Attached-wheel protocol mode.
 * @return True when the mode can source adapter clutch axes.
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
 * @return True when the mode supplies two clutch axes.
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

/**
 * @brief Converts encoded directional buttons to the PlayStation hat value.
 *
 * Permutes the four low input bits into the lookup order used by the PlayStation report and maps
 * invalid or neutral combinations to the corresponding table value.
 *
 * @param[in] directional_buttons Encoded attached-wheel directional buttons.
 * @return PlayStation hat value from zero through eight.
 */
uint8_t usb_playstation_input_map_hat(uint8_t directional_buttons) {
    uint8_t index =
        (uint8_t)(((directional_buttons & 0x01u) << 3) | ((directional_buttons >> 1) & 0x04u) |
                  (directional_buttons & 0x02u) | ((directional_buttons >> 2) & 0x01u));
    return playstation_hat_map[index];
}

/**
 * @brief Maps attached-wheel clutch controls to two PlayStation controller axes.
 *
 * Applies the mode-specific single-paddle, dual-paddle, or adapter source policy. Unsupported modes
 * and disconnected adapters produce centered axes.
 *
 * @param[out] axes Two clutch-axis output bytes.
 * @param[in] input Attached-wheel mode, paddle, capability, and adapter values.
 */
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
