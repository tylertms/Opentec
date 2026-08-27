#ifndef OPENTEC_BASE_SETTINGS_STATE_H
#define OPENTEC_BASE_SETTINGS_STATE_H

#include "profile/bank.h"
#include "wheel/position.h"

typedef struct {
    TuningProfileBank tuning_profiles;
    WheelPositionReference wheel_position;
} BaseSettings;

void base_settings_defaults(BaseSettings *settings);

#endif
