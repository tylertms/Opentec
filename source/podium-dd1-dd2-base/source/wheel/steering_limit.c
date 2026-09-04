#include "wheel/steering_limit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "profile/bank.h"
#include "usb/operating_mode_command.h"

/** @brief Operating-mode identifiers for steering-limit commands. */
enum {
    DEVICE_CONTROL_OPCODE = 1,        /**< Device-control operating-mode opcode. */
    STEERING_LIMIT_SELECTOR = 0x17,   /**< Steering-limit command selector. */
    STEERING_LIMIT_SET_OPERATION = 1, /**< Steering-limit set operation. */
};

/**
 * @brief Restores every profile steering limit to its default.
 *
 * Sets all six profile percentages to one hundred percent.
 *
 * @param[out] limits Per-profile steering-limit settings to initialize.
 */
void wheel_steering_limits_defaults(WheelSteeringLimits *limits) {
    for (uint8_t profile = 0; profile < TUNING_PROFILE_SLOT_COUNT; profile++) {
        limits->percent[profile] = WHEEL_STEERING_LIMIT_DEFAULT_PERCENT;
    }
}

/**
 * @brief Decodes a profile steering-limit device-control command.
 *
 * Accepts selector 0x17 only for its set operation. Percentages from zero through one hundred set
 * the active profile; larger values request a reset of every profile percentage.
 *
 * @param[in] source Decoded F8 09 operating-mode command.
 * @param[out] command Steering-limit value or reset request.
 * @return True when the opcode, selector, and operation identify a steering-limit command.
 */
bool wheel_steering_limit_command_decode(const UsbOperatingModeCommand *source,
                                         WheelSteeringLimitCommand *command) {
    if (source == NULL || command == NULL || source->opcode != DEVICE_CONTROL_OPCODE ||
        source->parameters[0] != STEERING_LIMIT_SELECTOR ||
        source->parameters[1] != STEERING_LIMIT_SET_OPERATION) {
        return false;
    }

    uint8_t percent = source->parameters[2];
    *command = (WheelSteeringLimitCommand){
        .percent = percent,
        .reset_all = percent > WHEEL_STEERING_LIMIT_DEFAULT_PERCENT,
    };
    return true;
}

/**
 * @brief Reads the steering limit for the active profile.
 *
 * Returns the selected profile percentage, or the default when settings are unavailable or the
 * profile index is invalid.
 *
 * @param[in] limits Per-profile steering-limit settings.
 * @param[in] active_profile Zero-based active profile index.
 * @return Active steering-limit percentage, or one hundred for an invalid input.
 */
uint8_t wheel_steering_limits_active(const WheelSteeringLimits *limits, uint8_t active_profile) {
    if (limits == NULL || active_profile >= TUNING_PROFILE_SLOT_COUNT) {
        return WHEEL_STEERING_LIMIT_DEFAULT_PERCENT;
    }
    return limits->percent[active_profile];
}
