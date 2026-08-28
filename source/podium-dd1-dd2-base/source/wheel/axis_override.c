#include "wheel/axis_override.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    OPERATING_MODE_FIXED_AXES = 0x1c,
    AXIS_AVAILABLE_THRESHOLD = 0x87,
    AXIS_UNAVAILABLE = 0xff,
};

static void clear_overrides(WheelAxisOverrides *overrides) {
    overrides->axis_5.enabled = false;
    overrides->axis_5.value = 0;
    overrides->axis_6.enabled = false;
    overrides->axis_6.value = 0;
    overrides->axis_7.enabled = false;
    overrides->axis_7.value = 0;
    overrides->auxiliary.enabled = false;
    overrides->auxiliary.value = 0;
}

static void convert_standard_axes(uint8_t axes[2]) {
    axes[0] = (uint8_t)(0x7fu - axes[0]);
    axes[1] = (uint8_t)(axes[1] + 0x80u);
}

static void convert_enabled_axes(uint8_t operating_mode, uint8_t axes[2]) {
    if (operating_mode == OPERATING_MODE_FIXED_AXES) {
        axes[0] = UINT8_MAX;
        axes[1] = 0;
        return;
    }
    convert_standard_axes(axes);
}

static bool interface_multiplexes_axes(uint8_t interface_mode) {
    return interface_mode >= WHEEL_AXIS_MULTIPLEX_INTERFACE_FIRST &&
           interface_mode <= WHEEL_AXIS_MULTIPLEX_INTERFACE_LAST;
}

static uint8_t encode_multiplexed_x(uint8_t interface_mode, uint8_t value) {
    value >>= 1;
    if (interface_mode == 6) {
        return (uint8_t)(value - 0x80u);
    }
    if (interface_mode == 7) {
        return (uint8_t)(value - 0x7fu);
    }
    return value;
}

static uint8_t encode_multiplexed_y(uint8_t interface_mode, uint8_t value) {
    value >>= 1;
    if (interface_mode == 6) {
        return (uint8_t)(value + 0x81u);
    }
    if (interface_mode == 7) {
        return (uint8_t)(value + 0x80u);
    }
    return value;
}

static void process_multiplexed_axes(WheelAxisOverrideProcessor *processor, uint8_t interface_mode,
                                     uint8_t x, uint8_t y, uint8_t axes[2]) {
    processor->x_available = x >= AXIS_AVAILABLE_THRESHOLD;
    processor->y_available = y >= AXIS_AVAILABLE_THRESHOLD;

    if (!interface_multiplexes_axes(interface_mode)) {
        axes[0] = (uint8_t)(x + 0x80u);
        axes[1] = (uint8_t)(y + 0x80u);
        return;
    }

    switch (processor->multiplex_phase) {
    case WHEEL_AXIS_MULTIPLEX_SELECT:
        if (x != AXIS_UNAVAILABLE) {
            processor->multiplex_phase = WHEEL_AXIS_MULTIPLEX_X;
        } else if (y != AXIS_UNAVAILABLE) {
            processor->multiplex_phase = WHEEL_AXIS_MULTIPLEX_Y;
        } else if (interface_mode == 7) {
            axes[0] = UINT8_MAX;
        }
        break;
    case WHEEL_AXIS_MULTIPLEX_X:
        if (x == AXIS_UNAVAILABLE) {
            processor->multiplex_phase = WHEEL_AXIS_MULTIPLEX_SELECT;
        } else {
            axes[0] = encode_multiplexed_x(interface_mode, x);
        }
        break;
    case WHEEL_AXIS_MULTIPLEX_Y:
        if (y == AXIS_UNAVAILABLE) {
            processor->multiplex_phase = WHEEL_AXIS_MULTIPLEX_SELECT;
        } else {
            axes[0] = encode_multiplexed_y(interface_mode, y);
        }
        break;
    default:
        processor->multiplex_phase = WHEEL_AXIS_MULTIPLEX_SELECT;
        break;
    }

    if (interface_mode == 7) {
        axes[1] = 0;
    }
}

/**
 * Clears the attached-wheel axis override processor.
 *
 * @param processor Axis override state to initialize.
 */
void wheel_axis_override_processor_init(WheelAxisOverrideProcessor *processor) {
    clear_overrides(&processor->overrides);
    processor->multiplex_phase = WHEEL_AXIS_MULTIPLEX_SELECT;
    processor->x_available = false;
    processor->y_available = false;
}

/**
 * Applies the attached-wheel axis mode to override outputs and report axes.
 *
 * @param processor Persistent override and multiplex state.
 * @param mode Selected axis override mode.
 * @param operating_mode Current device operating mode.
 * @param interface_mode Current input-report interface mode.
 * @param enabled True when the attached device provides axis overrides.
 * @param calibration_value Current calibrated axis value.
 * @param x First attached-device axis value.
 * @param y Second attached-device axis value.
 * @param axes Two report axes updated in place.
 */
void wheel_axis_override_process(WheelAxisOverrideProcessor *processor, uint8_t mode,
                                 uint8_t operating_mode, uint8_t interface_mode, bool enabled,
                                 uint8_t calibration_value, uint8_t x, uint8_t y, uint8_t axes[2]) {
    clear_overrides(&processor->overrides);
    if (!enabled) {
        convert_standard_axes(axes);
        return;
    }

    switch (mode) {
    case WHEEL_AXIS_OVERRIDE_MODE_CALIBRATED:
        processor->overrides.axis_7.enabled = true;
        processor->overrides.axis_7.value = calibration_value;
        convert_enabled_axes(operating_mode, axes);
        break;
    case WHEEL_AXIS_OVERRIDE_MODE_SECONDARY:
        processor->overrides.axis_7.enabled = true;
        processor->overrides.axis_7.value = x;
        processor->overrides.auxiliary.enabled = true;
        processor->overrides.auxiliary.value = y;
        convert_enabled_axes(operating_mode, axes);
        break;
    case WHEEL_AXIS_OVERRIDE_MODE_PRIMARY:
        processor->overrides.axis_6.enabled = true;
        processor->overrides.axis_6.value = x;
        processor->overrides.axis_5.enabled = true;
        processor->overrides.axis_5.value = y;
        convert_enabled_axes(operating_mode, axes);
        break;
    case WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED:
        process_multiplexed_axes(processor, interface_mode, x, y, axes);
        break;
    default:
        convert_standard_axes(axes);
        break;
    }
}
