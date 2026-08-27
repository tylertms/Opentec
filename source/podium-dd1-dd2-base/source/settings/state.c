#include "settings/state.h"

void base_settings_defaults(BaseSettings *settings) {
    tuning_profile_bank_defaults(&settings->tuning_profiles);
    wheel_position_reference_reset(&settings->wheel_position);
}
