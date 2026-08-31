#include "settings/state.h"

/**
 * @brief Restores every retained base setting to its startup value.
 *
 * Initializes tuning profiles, clears wheel and shifter calibration, requests auxiliary-axis
 * endpoint learning, disables the security code, enables attached-wheel auxiliary output, clears
 * compatibility globals, and marks the retained USB base mode unavailable.
 *
 * @param[out] settings Base settings record to initialize.
 */
void base_settings_defaults(BaseSettings *settings) {
    *settings = (BaseSettings){0};
    tuning_profile_bank_defaults(&settings->tuning_profiles);
    wheel_position_reference_reset(&settings->wheel_position);
    auxiliary_axis_settings_defaults(&settings->auxiliary_axis);
    wheel_steering_limits_defaults(&settings->steering_limits);
}
