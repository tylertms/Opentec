#ifndef OPENTEC_BASE_WHEEL_STEERING_LIMIT_H
#define OPENTEC_BASE_WHEEL_STEERING_LIMIT_H

#include <stdbool.h>
#include <stdint.h>

#include "profile/bank.h"
#include "usb/operating_mode_command.h"

/** @brief Default steering-limit percentage for every tuning profile. */
enum {
    WHEEL_STEERING_LIMIT_DEFAULT_PERCENT = 100 /**< Default percentage for every tuning profile. */
};

/** @brief Steering-limit percentage stored for each tuning profile. */
typedef struct {
    uint8_t percent[TUNING_PROFILE_SLOT_COUNT]; /**< Per-profile steering-limit percentages. */
} WheelSteeringLimits;

/** @brief Decoded steering-limit operating-mode command. */
typedef struct {
    uint8_t percent; /**< Percentage to apply when reset_all is false. */
    bool reset_all;  /**< True to restore every profile to the default percentage. */
} WheelSteeringLimitCommand;

/**
 * @brief Restores all steering limits to their defaults.
 *
 * Sets every tuning-profile percentage to WHEEL_STEERING_LIMIT_DEFAULT_PERCENT.
 *
 * @param[out] limits Steering-limit settings to initialize.
 */
void wheel_steering_limits_defaults(WheelSteeringLimits *limits);

/**
 * @brief Decodes a steering-limit operating-mode command.
 *
 * Accepts the device-control opcode, steering-limit selector, and set operation, and derives the
 * reset_all flag from the encoded percentage.
 *
 * @param[in] source Decoded F8 09 operating-mode command.
 * @param[out] command Steering-limit command to populate.
 * @return True when source identifies a steering-limit command and command is populated; otherwise
 * false.
 */
bool wheel_steering_limit_command_decode(const UsbOperatingModeCommand *source,
                                         WheelSteeringLimitCommand *command);

/**
 * @brief Returns the active profile's steering limit.
 *
 * Reads the selected profile percentage and uses the default percentage for a null settings object
 * or invalid profile index.
 *
 * @param[in] limits Per-profile steering-limit settings.
 * @param[in] active_profile Zero-based active profile index.
 * @return Selected profile percentage, or WHEEL_STEERING_LIMIT_DEFAULT_PERCENT when unavailable.
 */
uint8_t wheel_steering_limits_active(const WheelSteeringLimits *limits, uint8_t active_profile);

#endif
