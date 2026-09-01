#include "wheel/axis_override.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Attached-wheel axis, packet, and paddle-clutch protocol constants.
 *
 * These private values define packet field positions, unavailable-axis markers, analog thresholds,
 * and the timing used by the calibrated paddle-clutch adjustment sequence.
 */
enum {
    WHEEL_MODE_FIXED_AXES = 0x1c, /**< Wheel mode with fixed converted axis output. */
    WHEEL_MODE_CRC_AUTHENTICATED =
        0x15, /**< Authenticated CRC mode whose axes are already normalized. */
    WHEEL_MODE_STANDARD_RANGE_END = 0x12, /**< Last mode using the packet standard range. */
    AXIS_AVAILABLE_THRESHOLD = 0x87,      /**< Highest value treated as an available direct axis. */
    AXIS_UNAVAILABLE = 0xff,              /**< Marker for an unavailable axis source. */
    PACKET_AXIS_X = 5,       /**< Packet control index for the first analog-paddle axis. */
    PACKET_AXIS_Y = 6,       /**< Packet control index for the second analog-paddle axis. */
    PACKET_AXIS_ENABLED = 7, /**< Packet control index for the axis-report enable flag. */
    PACKET_AXIS_LIMIT_THRESHOLD =
        0x17,                       /**< Axis-limit value above which packet axes use full range. */
    PACKET_AXIS_VALUE_LIMIT = 0x7e, /**< Highest accepted half-range packet-axis value. */
    PADDLE_CLUTCH_PRESSED_THRESHOLD = 5, /**< Maximum active-low value treated as pressed. */
    PADDLE_CLUTCH_EXIT_THRESHOLD = 10,   /**< Value above which adjustment mode ends. */
    PADDLE_CLUTCH_RELEASED_THRESHOLD =
        0xf5, /**< Threshold above which a paddle is released for launch arming. */
    PADDLE_CLUTCH_PERCENT_MAXIMUM = 100,  /**< Maximum bite-point percentage. */
    PADDLE_ADJUST_BUTTON_INCREASE = 0x01, /**< Button bit requesting bite-point increase. */
    PADDLE_ADJUST_BUTTON_DECREASE = 0x08, /**< Button bit requesting bite-point decrease. */
    PADDLE_ADJUST_BUTTON_MASK = 0x09,     /**< Mask for the bite-point adjustment buttons. */
    PADDLE_ADJUST_CONSUMED_BUTTON_MASK =
        0x0f, /**< Button bits consumed during bite-point adjustment. */
    PADDLE_ADJUST_INITIAL_DELAY_MS = 1000, /**< Delay before the first held adjustment step. */
    PADDLE_ADJUST_REPEAT_DELAY_MS = 800,   /**< Delay between repeated held adjustment steps. */
};

/**
 * @brief Clears all attached-wheel pedal overrides.
 *
 * Disables each destination and resets its retained value.
 *
 * @param[out] overrides Pedal overrides to clear.
 */
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

/**
 * @brief Converts the two standard report axes to their centered output representation.
 *
 * Reverses the first axis around 0x7f and offsets the second axis by 0x80.
 *
 * @param[in,out] axes Two attached-wheel axes to convert.
 */
static void convert_standard_axes(uint8_t axes[2]) {
    axes[0] = (uint8_t)(0x7fu - axes[0]);
    axes[1] = (uint8_t)(axes[1] + 0x80u);
}

/**
 * @brief Converts report axes while attached-wheel overrides are enabled.
 *
 * Emits the fixed axis pair for wheel mode 0x1c and applies the standard conversion for every
 * other wheel mode.
 *
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @param[in,out] axes Two attached-wheel axes to convert.
 */
static void convert_enabled_axes(uint8_t wheel_mode, uint8_t axes[2]) {
    if (wheel_mode == WHEEL_MODE_FIXED_AXES) {
        axes[0] = UINT8_MAX;
        axes[1] = 0;
        return;
    }
    convert_standard_axes(axes);
}

/**
 * @brief Tests whether the host interface serializes two wheel axes onto one report axis.
 *
 * Selects interface modes six through eight.
 *
 * @param[in] interface_mode Current input-report interface mode.
 * @return True when the interface uses axis multiplexing.
 */
static bool interface_multiplexes_axes(uint8_t interface_mode) {
    return interface_mode >= WHEEL_AXIS_MULTIPLEX_INTERFACE_FIRST &&
           interface_mode <= WHEEL_AXIS_MULTIPLEX_INTERFACE_LAST;
}

/**
 * @brief Encodes the first wheel axis for a multiplexed host interface.
 *
 * Halves the source range and applies the interface-specific center offset.
 *
 * @param[in] interface_mode Current input-report interface mode.
 * @param[in] value First attached-wheel axis value.
 * @return Encoded multiplexed axis byte.
 */
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

/**
 * @brief Encodes the second wheel axis for a multiplexed host interface.
 *
 * Halves the source range and applies the interface-specific center offset.
 *
 * @param[in] interface_mode Current input-report interface mode.
 * @param[in] value Second attached-wheel axis value.
 * @return Encoded multiplexed axis byte.
 */
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

/**
 * @brief Advances direct-report wheel-axis multiplexing.
 *
 * Tracks axis availability, selects a non-sentinel source, and emits its interface-specific value
 * on subsequent protocol samples.
 *
 * @param[in,out] processor Persistent override and multiplex state.
 * @param[in] interface_mode Current input-report interface mode.
 * @param[in] x First attached-wheel axis value.
 * @param[in] y Second attached-wheel axis value.
 * @param[in,out] axes Two report axes updated in place.
 */
static void process_multiplexed_axes(WheelAxisOverrideProcessor *processor, uint8_t interface_mode,
                                     uint8_t x, uint8_t y, uint8_t axes[2]) {
    processor->x_available = x <= AXIS_AVAILABLE_THRESHOLD;
    processor->y_available = y <= AXIS_AVAILABLE_THRESHOLD;

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
 * @brief Scales a pressed clutch axis to the active bite-point percentage.
 *
 * Converts the active-low source to travel, scales that travel from zero through one hundred
 * percent, and converts the result back to an active-low axis.
 *
 * @param[in] value Active-low clutch axis value.
 * @param[in] percent Bite-point percentage from zero through one hundred.
 * @return Scaled active-low clutch axis value.
 */
static uint8_t scale_paddle_clutch(uint8_t value, uint8_t percent) {
    uint16_t travel = (uint8_t)~value;
    return (uint8_t)~(uint8_t)((travel * percent) / PADDLE_CLUTCH_PERCENT_MAXIMUM);
}

/**
 * @brief Combines two analog paddles into the calibrated clutch axis.
 *
 * Uses the more-pressed paddle during ordinary operation. Pressing both paddles arms the launch
 * sequence; releasing either paddle activates the configured bite point until both are released.
 *
 * @param[in,out] processor Persistent paddle-clutch phase.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in,out] bite_point_percent Active profile bite-point percentage.
 * @param[in,out] buttons Primary attached-wheel button bank.
 * @param[in,out] motion Primary attached-wheel rotary motion.
 * @param[in] x First analog-paddle value.
 * @param[in] y Second analog-paddle value.
 * @return Combined active-low clutch axis value.
 */
static uint8_t process_paddle_clutch(WheelAxisOverrideProcessor *processor, uint32_t now_ms,
                                     uint8_t *bite_point_percent, uint8_t *buttons, int8_t *motion,
                                     uint8_t x, uint8_t y) {
    uint8_t lower = x < y ? x : y;
    uint8_t upper = x < y ? y : x;
    uint8_t output = lower;

    switch (processor->paddle_clutch_phase) {
    case WHEEL_PADDLE_CLUTCH_IDLE:
        if (lower <= PADDLE_CLUTCH_PRESSED_THRESHOLD && upper <= PADDLE_CLUTCH_PRESSED_THRESHOLD) {
            processor->paddle_clutch_phase = WHEEL_PADDLE_CLUTCH_ARMED;
        } else if (lower <= PADDLE_CLUTCH_PRESSED_THRESHOLD &&
                   (*buttons & PADDLE_ADJUST_BUTTON_MASK) != 0) {
            processor->paddle_clutch_phase = WHEEL_PADDLE_CLUTCH_ADJUSTING;
            processor->paddle_adjustment_deadline_ms = now_ms + PADDLE_ADJUST_INITIAL_DELAY_MS;
            *buttons &= (uint8_t)~PADDLE_ADJUST_BUTTON_MASK;
        }
        break;
    case WHEEL_PADDLE_CLUTCH_ADJUSTING:
        if ((int32_t)(now_ms - processor->paddle_adjustment_deadline_ms) >= 0) {
            if ((*buttons & PADDLE_ADJUST_BUTTON_INCREASE) != 0 || *motion > 0) {
                if (*bite_point_percent < PADDLE_CLUTCH_PERCENT_MAXIMUM) {
                    (*bite_point_percent)++;
                    processor->paddle_bite_point_report_pending = true;
                }
                processor->paddle_adjustment_deadline_ms = now_ms + PADDLE_ADJUST_REPEAT_DELAY_MS;
            } else if ((*buttons & PADDLE_ADJUST_BUTTON_DECREASE) != 0 || *motion < 0) {
                if (*bite_point_percent != 0) {
                    (*bite_point_percent)--;
                    processor->paddle_bite_point_report_pending = true;
                }
                processor->paddle_adjustment_deadline_ms = now_ms + PADDLE_ADJUST_REPEAT_DELAY_MS;
            }
        }
        if ((*buttons & PADDLE_ADJUST_BUTTON_MASK) == 0 && *motion == 0) {
            processor->paddle_adjustment_deadline_ms = now_ms;
        }
        output = scale_paddle_clutch(0, *bite_point_percent);
        *buttons &= (uint8_t)~PADDLE_ADJUST_CONSUMED_BUTTON_MASK;
        *motion = 0;
        if (lower > PADDLE_CLUTCH_EXIT_THRESHOLD) {
            processor->paddle_clutch_phase = WHEEL_PADDLE_CLUTCH_IDLE;
            processor->paddle_bite_point_commit_pending = true;
        }
        break;
    case WHEEL_PADDLE_CLUTCH_ARMED:
        if (upper > PADDLE_CLUTCH_RELEASED_THRESHOLD) {
            processor->paddle_clutch_phase = WHEEL_PADDLE_CLUTCH_ACTIVE;
        }
        break;
    case WHEEL_PADDLE_CLUTCH_ACTIVE:
        output = scale_paddle_clutch(lower, *bite_point_percent);
        if (lower == UINT8_MAX) {
            processor->paddle_clutch_phase = WHEEL_PADDLE_CLUTCH_IDLE;
        }
        break;
    default:
        processor->paddle_clutch_phase = WHEEL_PADDLE_CLUTCH_IDLE;
        break;
    }
    return output;
}

/**
 * @brief Normalizes one CRC-family analog-paddle axis.
 *
 * Reverses full-range values and maps half-range values through their doubled active-low range.
 * Values above the accepted half-range limit map to zero.
 *
 * @param[in] value Encoded analog-paddle axis value.
 * @param[in] full_range True when the packet supplies the full byte range.
 * @return Normalized active-low axis value.
 */
static uint8_t normalize_packet_axis(uint8_t value, bool full_range) {
    if (full_range) {
        return (uint8_t)~value;
    }
    return value > PACKET_AXIS_VALUE_LIMIT ? 0 : (uint8_t)~(uint8_t)(value * 2u);
}

/**
 * @brief Normalizes both CRC-family analog-paddle controls.
 *
 * Preserves authenticated mode 0x15 values and selects full- or half-range conversion for other
 * packet modes from their axis limit and wheel mode.
 *
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @param[in] axis_limit Attached-wheel axis-limit capability value.
 * @param[in,out] controls Eight CRC-family control bytes.
 */
static void normalize_packet_axes(uint8_t wheel_mode, uint8_t axis_limit, uint8_t controls[8]) {
    if (wheel_mode == WHEEL_MODE_CRC_AUTHENTICATED) {
        return;
    }
    bool full_range =
        axis_limit > PACKET_AXIS_LIMIT_THRESHOLD || wheel_mode > WHEEL_MODE_STANDARD_RANGE_END;
    controls[PACKET_AXIS_X] = normalize_packet_axis(controls[PACKET_AXIS_X], full_range);
    controls[PACKET_AXIS_Y] = normalize_packet_axis(controls[PACKET_AXIS_Y], full_range);
}

/**
 * @brief Publishes pedal overrides from CRC-family analog-paddle controls.
 *
 * Maps the selected paddle mode to its pedal destinations and centers the consumed packet axes.
 *
 * @param[in,out] processor Persistent override and clutch state.
 * @param[in] mode Selected axis override mode.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in,out] bite_point_percent Active profile bite-point percentage.
 * @param[in,out] buttons Primary attached-wheel button bank.
 * @param[in,out] motion Primary attached-wheel rotary motion.
 * @param[in,out] controls Eight CRC-family control bytes.
 */
static void publish_packet_overrides(WheelAxisOverrideProcessor *processor, uint8_t mode,
                                     uint32_t now_ms, uint8_t *bite_point_percent, uint8_t *buttons,
                                     int8_t *motion, uint8_t controls[8]) {
    uint8_t x = controls[PACKET_AXIS_X];
    uint8_t y = controls[PACKET_AXIS_Y];
    clear_overrides(&processor->overrides);
    switch (mode) {
    case WHEEL_AXIS_OVERRIDE_MODE_CALIBRATED:
        processor->overrides.axis_7.enabled = true;
        processor->overrides.axis_7.value =
            process_paddle_clutch(processor, now_ms, bite_point_percent, buttons, motion, x, y);
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

/**
 * @brief Advances CRC-family wheel-axis multiplexing.
 *
 * Selects an available packet axis, emits its interface-specific value, and centers the consumed
 * control fields for nonmultiplexed interfaces.
 *
 * @param[in,out] processor Persistent override and multiplex state.
 * @param[in] interface_mode Current input-report interface mode.
 * @param[in,out] controls Eight CRC-family control bytes.
 * @param[in,out] axes Two packet output axes updated in place.
 */
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
            axes[0] = x >> 1;
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
 * @brief Selects the inactive output used by axis-mode packets.
 *
 * Emits the multiplex-interface sentinel pair for interface modes six through eight and clears
 * both axes for every other interface.
 *
 * @param[in] interface_mode Current input-report interface mode.
 * @param[out] axes Two packet output axes.
 */
static void set_axis_mode_inactive_output(uint8_t interface_mode, uint8_t axes[2]) {
    axes[0] = interface_multiplexes_axes(interface_mode) ? UINT8_MAX : 0;
    axes[1] = 0;
}

/**
 * @brief Advances axis-mode packet multiplexing.
 *
 * Tracks the two filtered controls, emits centered independent axes on nonmultiplexed interfaces,
 * and serializes an available source through the interface-specific primary axis otherwise.
 *
 * @param[in,out] processor Persistent availability and multiplex phase.
 * @param[in] interface_mode Current input-report interface mode.
 * @param[in] x First filtered axis-mode control.
 * @param[in] y Second filtered axis-mode control.
 * @param[out] axes Two packet output axes.
 */
static void process_axis_mode_multiplexed_axes(WheelAxisOverrideProcessor *processor,
                                               uint8_t interface_mode, uint8_t x, uint8_t y,
                                               uint8_t axes[2]) {
    processor->x_available = x <= AXIS_AVAILABLE_THRESHOLD;
    processor->y_available = y <= AXIS_AVAILABLE_THRESHOLD;
    if (!interface_multiplexes_axes(interface_mode)) {
        axes[0] = (uint8_t)(x + 0x80u);
        axes[1] = (uint8_t)(y + 0x80u);
        return;
    }

    axes[0] = 0x80;
    axes[1] = 0;
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
            axes[0] = x >> 1;
        }
        break;
    case WHEEL_AXIS_MULTIPLEX_Y:
        if (y == AXIS_UNAVAILABLE) {
            processor->multiplex_phase = WHEEL_AXIS_MULTIPLEX_SELECT;
        } else if (interface_mode == 6) {
            axes[0] = (uint8_t)~((uint16_t)y >> 1);
        } else if (interface_mode == 7) {
            axes[0] = (uint8_t)(0x80u + ((uint16_t)y >> 1));
        } else {
            axes[0] = y >> 1;
        }
        break;
    default:
        processor->multiplex_phase = WHEEL_AXIS_MULTIPLEX_SELECT;
        break;
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
    processor->paddle_adjustment_deadline_ms = 0;
    processor->multiplex_phase = WHEEL_AXIS_MULTIPLEX_SELECT;
    processor->paddle_clutch_phase = WHEEL_PADDLE_CLUTCH_IDLE;
    processor->x_available = false;
    processor->y_available = false;
    processor->packet_axis_report_enabled = false;
    processor->paddle_bite_point_report_pending = false;
    processor->paddle_bite_point_commit_pending = false;
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
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in,out] bite_point_percent Active profile bite-point percentage.
 * @param[in,out] buttons Primary attached-wheel button bank.
 * @param[in,out] motion Primary attached-wheel rotary motion.
 * @param[in] x First attached-device axis value.
 * @param[in] y Second attached-device axis value.
 * @param[in,out] axes Two report axes updated in place.
 */
void wheel_axis_override_process(WheelAxisOverrideProcessor *processor, uint8_t mode,
                                 uint8_t wheel_mode, uint8_t interface_mode, bool enabled,
                                 uint32_t now_ms, uint8_t *bite_point_percent, uint8_t *buttons,
                                 int8_t *motion, uint8_t x, uint8_t y, uint8_t axes[2]) {
    clear_overrides(&processor->overrides);
    if (!enabled) {
        convert_standard_axes(axes);
        return;
    }

    switch (mode) {
    case WHEEL_AXIS_OVERRIDE_MODE_CALIBRATED:
        processor->overrides.axis_7.enabled = true;
        processor->overrides.axis_7.value =
            process_paddle_clutch(processor, now_ms, bite_point_percent, buttons, motion, x, y);
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
 * @brief Applies a packet family's axis-control mode.
 *
 * Normalizes the packet axis bytes, updates the persistent availability state, publishes the
 * selected overrides, or advances the packet multiplex phase and output axes.
 *
 * @param[in,out] processor Persistent override, availability, and multiplex state.
 * @param[in] mode Selected axis-control mode.
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @param[in] interface_mode Current input-report interface mode.
 * @param[in] axis_limit Attached-wheel axis-limit capability value.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in,out] bite_point_percent Active profile bite-point percentage.
 * @param[in,out] buttons Primary attached-wheel button bank.
 * @param[in,out] motion Primary attached-wheel rotary motion.
 * @param[in,out] controls Eight packet control bytes updated in place.
 * @param[in,out] axes Two packet output axes updated in place when required by the mode.
 */
void wheel_axis_override_process_packet(WheelAxisOverrideProcessor *processor, uint8_t mode,
                                        uint8_t wheel_mode, uint8_t interface_mode,
                                        uint8_t axis_limit, uint32_t now_ms,
                                        uint8_t *bite_point_percent, uint8_t *buttons,
                                        int8_t *motion, uint8_t controls[8], uint8_t axes[2]) {
    normalize_packet_axes(wheel_mode, axis_limit, controls);
    if (wheel_mode == WHEEL_MODE_CRC_AUTHENTICATED && controls[PACKET_AXIS_ENABLED] != 1) {
        processor->packet_axis_report_enabled = false;
    }
    if (controls[PACKET_AXIS_ENABLED] == 1) {
        processor->packet_axis_report_enabled = true;
    }

    if (mode >= WHEEL_AXIS_OVERRIDE_MODE_CALIBRATED && mode <= WHEEL_AXIS_OVERRIDE_MODE_PRIMARY) {
        publish_packet_overrides(processor, mode, now_ms, bite_point_percent, buttons, motion,
                                 controls);
    } else if (mode == WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED) {
        process_packet_multiplexed_axes(processor, interface_mode, controls, axes);
    }
}

/**
 * @brief Applies an axis-mode packet's selected override behavior.
 *
 * Maps the two filtered controls into the selected pedal destinations, runs calibrated clutch
 * policy when requested, or advances the packet family's axis multiplexer. Unsupported selectors
 * disable overrides and publish the interface's inactive axis pair.
 *
 * @param[in,out] processor Persistent override, clutch, availability, and multiplex state.
 * @param[in] mode Axis-control selector carried by the packet.
 * @param[in] interface_mode Current input-report interface mode.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in,out] bite_point_percent Active profile bite-point percentage.
 * @param[in,out] buttons Primary attached-wheel button bank.
 * @param[in,out] motion Primary attached-wheel rotary motion.
 * @param[in] controls Eight filtered axis-mode control bytes.
 * @param[out] axes Two packet output axes.
 */
void wheel_axis_override_process_axis_mode(WheelAxisOverrideProcessor *processor, uint8_t mode,
                                           uint8_t interface_mode, uint32_t now_ms,
                                           uint8_t *bite_point_percent, uint8_t *buttons,
                                           int8_t *motion, const uint8_t controls[8],
                                           uint8_t axes[2]) {
    uint8_t x = controls[4];
    uint8_t y = controls[5];
    clear_overrides(&processor->overrides);
    switch (mode) {
    case WHEEL_AXIS_OVERRIDE_MODE_CALIBRATED:
        processor->overrides.axis_7.enabled = true;
        processor->overrides.axis_7.value =
            process_paddle_clutch(processor, now_ms, bite_point_percent, buttons, motion, x, y);
        set_axis_mode_inactive_output(interface_mode, axes);
        break;
    case WHEEL_AXIS_OVERRIDE_MODE_SECONDARY:
        processor->overrides.axis_7.enabled = true;
        processor->overrides.axis_7.value = x;
        processor->overrides.auxiliary.enabled = true;
        processor->overrides.auxiliary.value = y;
        set_axis_mode_inactive_output(interface_mode, axes);
        break;
    case WHEEL_AXIS_OVERRIDE_MODE_PRIMARY:
        processor->overrides.axis_6.enabled = true;
        processor->overrides.axis_6.value = x;
        processor->overrides.axis_5.enabled = true;
        processor->overrides.axis_5.value = y;
        set_axis_mode_inactive_output(interface_mode, axes);
        break;
    case WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED:
        process_axis_mode_multiplexed_axes(processor, interface_mode, x, y, axes);
        break;
    default:
        set_axis_mode_inactive_output(interface_mode, axes);
        break;
    }
}

/**
 * @brief Takes a completed wheel-side bite-point adjustment.
 *
 * Returns the adjusted percentage once after the analog paddle is released from adjustment mode.
 *
 * @param[in,out] processor Persistent paddle-clutch state.
 * @param[in] bite_point_percent Current adjusted bite-point percentage.
 * @param[out] updated_percent Completed percentage to persist.
 * @return true when a completed adjustment was available and copied; false otherwise.
 */
bool wheel_axis_override_take_bite_point(WheelAxisOverrideProcessor *processor,
                                         uint8_t bite_point_percent, uint8_t *updated_percent) {
    if (!processor->paddle_bite_point_commit_pending) {
        return false;
    }
    processor->paddle_bite_point_commit_pending = false;
    *updated_percent = bite_point_percent;
    return true;
}

/**
 * @brief Takes a wheel-side bite-point report update.
 *
 * Returns the adjusted percentage once after an accepted increment or decrement.
 *
 * @param[in,out] processor Persistent paddle-clutch state.
 * @param[in] bite_point_percent Current adjusted bite-point percentage.
 * @param[out] updated_percent Percentage to publish in the next input report.
 * @return true when a new percentage was available and copied; false otherwise.
 */
bool wheel_axis_override_take_bite_point_report(WheelAxisOverrideProcessor *processor,
                                                uint8_t bite_point_percent,
                                                uint8_t *updated_percent) {
    if (!processor->paddle_bite_point_report_pending) {
        return false;
    }
    processor->paddle_bite_point_report_pending = false;
    *updated_percent = bite_point_percent;
    return true;
}
