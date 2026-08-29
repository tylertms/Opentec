#include "wheel/packet_axis_mode.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    WHEEL_PACKET_AXIS_MODE_STANDARD = 0x09,
    WHEEL_PACKET_AXIS_MODE_AUTHENTICATED = 0x0b,
    WHEEL_PACKET_AXIS_MODE_EXTENDED = 0x1d,
    AXIS_X_CONTROL = 4,
    PACKED_OUTPUT_CONTROL = 7,
};

/**
 * @brief Reports whether a wheel mode uses the shared axis-mode packet policy.
 *
 * Selects the standard, authenticated, and extended variants that share the common payload,
 * three-sample input filter, and packet-selected axis behavior.
 *
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @return True for mode 0x09, mode 0x0B, or mode 0x1D.
 */
bool wheel_packet_axis_mode_applies(uint8_t wheel_mode) {
    return wheel_mode == WHEEL_PACKET_AXIS_MODE_STANDARD ||
           wheel_mode == WHEEL_PACKET_AXIS_MODE_AUTHENTICATED ||
           wheel_mode == WHEEL_PACKET_AXIS_MODE_EXTENDED;
}

/**
 * @brief Clears the axis-mode packet histories.
 *
 * Zeros the three button and analog-axis samples and returns the shared insertion position to the
 * first sample.
 *
 * @param[out] filter Axis-mode filter state to initialize.
 */
void wheel_packet_axis_mode_filter_init(WheelPacketAxisModeFilter *filter) {
    for (uint8_t sample = 0; sample < WHEEL_PACKET_AXIS_MODE_HISTORY_DEPTH; sample++) {
        for (uint8_t button = 0; button < WHEEL_PACKET_COMMON_BUTTON_COUNT; button++) {
            filter->button_samples[sample][button] = 0;
        }
        for (uint8_t axis = 0; axis < WHEEL_PACKET_AXIS_MODE_AXIS_COUNT; axis++) {
            filter->axis_samples[sample][axis] = 0;
        }
    }
    filter->next_sample = 0;
}

/**
 * @brief Filters axis-mode buttons and analog axes.
 *
 * Keeps button bits present in all three recent samples and replaces the two analog controls with
 * their unsigned three-sample means. Empty startup samples contribute zero.
 *
 * @param[in,out] filter Shared button and analog-axis histories.
 * @param[in,out] input Decoded axis-mode input filtered in place.
 */
void wheel_packet_axis_mode_filter(WheelPacketAxisModeFilter *filter,
                                   WheelPacketAxisModeInput *input) {
    uint8_t sample = filter->next_sample;
    for (uint8_t button = 0; button < WHEEL_PACKET_COMMON_BUTTON_COUNT; button++) {
        filter->button_samples[sample][button] = input->buttons[button];
        input->buttons[button] = filter->button_samples[0][button] &
                                 filter->button_samples[1][button] &
                                 filter->button_samples[2][button];
    }
    for (uint8_t axis = 0; axis < WHEEL_PACKET_AXIS_MODE_AXIS_COUNT; axis++) {
        filter->axis_samples[sample][axis] = input->controls[AXIS_X_CONTROL + axis];
        uint16_t total = 0;
        for (uint8_t history = 0; history < WHEEL_PACKET_AXIS_MODE_HISTORY_DEPTH; history++) {
            total += filter->axis_samples[history][axis];
        }
        input->controls[AXIS_X_CONTROL + axis] =
            (uint8_t)(total / WHEEL_PACKET_AXIS_MODE_HISTORY_DEPTH);
    }
    filter->next_sample++;
    if (filter->next_sample == WHEEL_PACKET_AXIS_MODE_HISTORY_DEPTH) {
        filter->next_sample = 0;
    }
}

/**
 * @brief Expands the packed axis-mode output controls.
 *
 * Replaces the first two control bytes with the low and high nibbles of control byte seven while
 * preserving the packet's axis selector and filtered analog controls.
 *
 * @param[in,out] input Filtered axis-mode input updated in place.
 */
void wheel_packet_axis_mode_expand_controls(WheelPacketAxisModeInput *input) {
    input->controls[0] = input->controls[PACKED_OUTPUT_CONTROL] & 0x0fu;
    input->controls[1] = input->controls[PACKED_OUTPUT_CONTROL] >> 4;
}
