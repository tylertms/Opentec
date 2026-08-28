#include "wheel/axis_override.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    WHEEL_MODE_FIXED_AXES = 0x1c,
    WHEEL_MODE_CRC_AUTHENTICATED = 0x15,
    WHEEL_MODE_STANDARD_RANGE_END = 0x12,
    AXIS_AVAILABLE_THRESHOLD = 0x87,
    AXIS_UNAVAILABLE = 0xff,
    PACKET_AXIS_X = 5,
    PACKET_AXIS_Y = 6,
    PACKET_AXIS_ENABLED = 7,
    PACKET_AXIS_LIMIT_THRESHOLD = 0x17,
    PACKET_AXIS_VALUE_LIMIT = 0x7e,
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

static void convert_enabled_axes(uint8_t wheel_mode, uint8_t axes[2]) {
    if (wheel_mode == WHEEL_MODE_FIXED_AXES) {
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

static uint8_t normalize_packet_axis(uint8_t value, bool full_range) {
    if (full_range) {
        return (uint8_t)~value;
    }
    return value > PACKET_AXIS_VALUE_LIMIT ? 0 : (uint8_t)~(uint8_t)(value * 2u);
}

static void normalize_packet_axes(uint8_t wheel_mode, uint8_t axis_limit, uint8_t controls[8]) {
    if (wheel_mode == WHEEL_MODE_CRC_AUTHENTICATED) {
        return;
    }
    bool full_range =
        axis_limit > PACKET_AXIS_LIMIT_THRESHOLD || wheel_mode > WHEEL_MODE_STANDARD_RANGE_END;
    controls[PACKET_AXIS_X] = normalize_packet_axis(controls[PACKET_AXIS_X], full_range);
    controls[PACKET_AXIS_Y] = normalize_packet_axis(controls[PACKET_AXIS_Y], full_range);
}

static void publish_packet_overrides(WheelAxisOverrideProcessor *processor, uint8_t mode,
                                     uint8_t calibration_value, uint8_t controls[8]) {
    uint8_t x = controls[PACKET_AXIS_X];
    uint8_t y = controls[PACKET_AXIS_Y];
    clear_overrides(&processor->overrides);
    switch (mode) {
    case WHEEL_AXIS_OVERRIDE_MODE_CALIBRATED:
        processor->overrides.axis_7.enabled = true;
        processor->overrides.axis_7.value = calibration_value;
        break;
    case WHEEL_AXIS_OVERRIDE_MODE_SECONDARY:
        processor->overrides.axis_7.enabled = true;
        processor->overrides.axis_7.value = x;
        processor->overrides.auxiliary.enabled = true;
        processor->overrides.auxiliary.value = y;
        break;
    case WHEEL_AXIS_OVERRIDE_MODE_PRIMARY:
        processor->overrides.axis_6.enabled = true;
        processor->overrides.axis_6.value = x;
        processor->overrides.axis_5.enabled = true;
        processor->overrides.axis_5.value = y;
        break;
    }
    controls[PACKET_AXIS_X] = 0x80;
    controls[PACKET_AXIS_Y] = 0x80;
}

static void process_packet_multiplexed_axes(WheelAxisOverrideProcessor *processor,
                                            uint8_t interface_mode, uint8_t controls[8],
                                            uint8_t axes[2]) {
    uint8_t x = controls[PACKET_AXIS_X];
    uint8_t y = controls[PACKET_AXIS_Y];
    clear_overrides(&processor->overrides);
    if (!interface_multiplexes_axes(interface_mode)) {
        axes[0] = (uint8_t)(x + 0x80u);
        axes[1] = (uint8_t)(y + 0x80u);
        controls[PACKET_AXIS_X] = axes[0];
        controls[PACKET_AXIS_Y] = axes[1];
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
        } else if (interface_mode == 6) {
            axes[0] = (uint8_t)(0x80u - ((uint16_t)x >> 1));
        } else if (interface_mode == 7) {
            axes[0] = (uint8_t)(0x7fu - ((uint16_t)x >> 1));
        } else {
            axes[0] = (uint8_t)((uint16_t)x >> 1);
        }
        break;
    case WHEEL_AXIS_MULTIPLEX_Y:
        if (y == AXIS_UNAVAILABLE) {
            processor->multiplex_phase = WHEEL_AXIS_MULTIPLEX_SELECT;
        } else if (interface_mode == 6) {
            axes[0] = (uint8_t)(0x81u + ((uint16_t)y >> 1));
        } else if (interface_mode == 7) {
            axes[0] = (uint8_t)(0x80u + ((uint16_t)y >> 1));
        } else {
            axes[0] = (uint8_t)~((uint16_t)y >> 1);
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
 * @brief Clears the attached-wheel axis override processor.
 *
 * Clears every override and availability flag and returns axis multiplexing to its selection
 * phase.
 *
 * @param[out] processor Axis override state to initialize.
 */
void wheel_axis_override_processor_init(WheelAxisOverrideProcessor *processor) {
    clear_overrides(&processor->overrides);
    processor->multiplex_phase = WHEEL_AXIS_MULTIPLEX_SELECT;
    processor->x_available = false;
    processor->y_available = false;
    processor->packet_axis_report_enabled = false;
}

/**
 * @brief Applies the attached-wheel axis override mode.
 *
 * Clears prior overrides, maps enabled wheel axes into the selected output destinations, and
 * converts or multiplexes the two report axes.
 *
 * @param[in,out] processor Persistent override and multiplex state.
 * @param[in] mode Selected axis override mode.
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @param[in] interface_mode Current input-report interface mode.
 * @param[in] enabled True when the attached device provides axis overrides.
 * @param[in] calibration_value Current calibrated axis value.
 * @param[in] x First attached-device axis value.
 * @param[in] y Second attached-device axis value.
 * @param[in,out] axes Two report axes updated in place.
 */
void wheel_axis_override_process(WheelAxisOverrideProcessor *processor, uint8_t mode,
                                 uint8_t wheel_mode, uint8_t interface_mode, bool enabled,
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
        convert_enabled_axes(wheel_mode, axes);
        break;
    case WHEEL_AXIS_OVERRIDE_MODE_SECONDARY:
        processor->overrides.axis_7.enabled = true;
        processor->overrides.axis_7.value = x;
        processor->overrides.auxiliary.enabled = true;
        processor->overrides.auxiliary.value = y;
        convert_enabled_axes(wheel_mode, axes);
        break;
    case WHEEL_AXIS_OVERRIDE_MODE_PRIMARY:
        processor->overrides.axis_6.enabled = true;
        processor->overrides.axis_6.value = x;
        processor->overrides.axis_5.enabled = true;
        processor->overrides.axis_5.value = y;
        convert_enabled_axes(wheel_mode, axes);
        break;
    case WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED:
        process_multiplexed_axes(processor, interface_mode, x, y, axes);
        break;
    default:
        convert_standard_axes(axes);
        break;
    }
}

/**
 * @brief Applies the CRC-family packet axis-control mode.
 *
 * Normalizes the packet axis bytes, updates the persistent availability state, publishes the
 * selected overrides, or advances the packet multiplex phase and output axes.
 *
 * @param[in,out] processor Persistent override, availability, and multiplex state.
 * @param[in] mode Selected axis-control mode.
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @param[in] interface_mode Current input-report interface mode.
 * @param[in] axis_limit Attached-wheel axis-limit capability value.
 * @param[in] calibration_value Current calibrated axis value.
 * @param[in,out] controls Eight CRC-family control bytes updated in place.
 * @param[in,out] axes Two packet output axes updated in place when required by the mode.
 */
void wheel_axis_override_process_packet(WheelAxisOverrideProcessor *processor, uint8_t mode,
                                        uint8_t wheel_mode, uint8_t interface_mode,
                                        uint8_t axis_limit, uint8_t calibration_value,
                                        uint8_t controls[8], uint8_t axes[2]) {
    normalize_packet_axes(wheel_mode, axis_limit, controls);
    if (wheel_mode == WHEEL_MODE_CRC_AUTHENTICATED && controls[PACKET_AXIS_ENABLED] != 1) {
        processor->packet_axis_report_enabled = false;
    }
    if (controls[PACKET_AXIS_ENABLED] == 1) {
        processor->packet_axis_report_enabled = true;
    }

    if (mode >= WHEEL_AXIS_OVERRIDE_MODE_CALIBRATED && mode <= WHEEL_AXIS_OVERRIDE_MODE_PRIMARY) {
        publish_packet_overrides(processor, mode, calibration_value, controls);
    } else if (mode == WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED) {
        process_packet_multiplexed_axes(processor, interface_mode, controls, axes);
    }
}
