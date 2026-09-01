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
 * @brief Applies a profile steering-limit command.
 *
 * Updates the active profile for a bounded percentage or restores all six profile values for a
 * reset request. Invalid profile indices leave the settings unchanged.
 *
 * @param[in,out] limits Per-profile steering-limit settings.
 * @param[in] active_profile Zero-based active profile index.
 * @param[in] command Steering-limit value or reset request.
 * @return Changed when at least one stored percentage changed; otherwise unchanged.
 */
WheelSteeringLimitResult wheel_steering_limits_apply(WheelSteeringLimits *limits,
                                                     uint8_t active_profile,
                                                     const WheelSteeringLimitCommand *command) {
    if (limits == NULL || command == NULL) {
        return WHEEL_STEERING_LIMIT_UNCHANGED;
    }

    if (command->reset_all) {
        bool changed = false;
        for (uint8_t profile = 0; profile < TUNING_PROFILE_SLOT_COUNT; profile++) {
            changed |= limits->percent[profile] != WHEEL_STEERING_LIMIT_DEFAULT_PERCENT;
            limits->percent[profile] = WHEEL_STEERING_LIMIT_DEFAULT_PERCENT;
        }
        return changed ? WHEEL_STEERING_LIMIT_CHANGED : WHEEL_STEERING_LIMIT_UNCHANGED;
    }

    if (active_profile >= TUNING_PROFILE_SLOT_COUNT ||
        limits->percent[active_profile] == command->percent) {
        return WHEEL_STEERING_LIMIT_UNCHANGED;
    }
    limits->percent[active_profile] = command->percent;
    return WHEEL_STEERING_LIMIT_CHANGED;
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
