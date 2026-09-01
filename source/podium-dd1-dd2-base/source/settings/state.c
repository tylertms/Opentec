#include "settings/state.h"

void base_settings_defaults(BaseSettings *settings) {
    *settings = (BaseSettings){0};
    tuning_profile_bank_defaults(&settings->tuning_profiles);
    wheel_position_reference_reset(&settings->wheel_position);
    auxiliary_axis_settings_defaults(&settings->auxiliary_axis);
    wheel_steering_limits_defaults(&settings->steering_limits);
}
