#ifndef OPENTEC_BASE_SETTINGS_STATE_H
#define OPENTEC_BASE_SETTINGS_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "analog/auxiliary_axis.h"
#include "profile/bank.h"
#include "security/code.h"
#include "shifter/h_pattern.h"
#include "wheel/position.h"
#include "wheel/steering_limit.h"

/** @brief Number of 16-bit values in one retained tuning-profile block. */
enum { BASE_SETTINGS_PROFILE_STORED_VALUE_COUNT = 26 };

/** @brief Complete retained base settings state. */
typedef struct {
    TuningProfileBank tuning_profiles;     /**< Retained tuning profiles and mode. */
    WheelPositionReference wheel_position; /**< Wheel-center reference and calibration state. */
    HPatternSettings h_pattern_shifter;    /**< H-pattern shifter settings. */
    AuxiliaryAxisSettings auxiliary_axis;  /**< Auxiliary-axis calibration settings. */
    WheelSteeringLimits steering_limits;   /**< Per-profile steering limits. */
    SecurityCodeSettings security_code;    /**< Retained security-code settings. */
    uint16_t retained_global_values[2];    /**< Retained compatibility values. */
    uint16_t retained_profile_words[TUNING_PROFILE_SLOT_COUNT]
                                  [BASE_SETTINGS_PROFILE_STORED_VALUE_COUNT];
    /**< Raw 16-bit profile words, including values not exposed by the runtime profile. */
    uint8_t operating_mode;                                 /**< Retained operating-mode value. */
    uint8_t wheel_auxiliary_option; /**< Retained attached-wheel auxiliary option. */
    bool operating_mode_valid;      /**< True when operating_mode has a valid retained marker. */
} BaseSettings;

/**
 * @brief Restores base settings to startup defaults.
 *
 * Initializes tuning, calibration, security, auxiliary, and compatibility state for a fresh device.
 *
 * @param[out] settings Base settings record to initialize.
 */
void base_settings_defaults(BaseSettings *settings);

#endif
