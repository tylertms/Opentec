#include "settings/state.h"

/**
 * @brief Restores every retained base setting to its startup value.
 *
 * Initializes tuning profiles, clears wheel and shifter calibration, and requests auxiliary-axis
 * endpoint learning.
 *
 * @param[out] settings Base settings record to initialize.
 */
void base_settings_defaults(BaseSettings *settings) {
    tuning_profile_bank_defaults(&settings->tuning_profiles);
    wheel_position_reference_reset(&settings->wheel_position);
    settings->h_pattern_shifter = (HPatternSettings){0};
    auxiliary_axis_settings_defaults(&settings->auxiliary_axis);
    wheel_steering_limits_defaults(&settings->steering_limits);
}
