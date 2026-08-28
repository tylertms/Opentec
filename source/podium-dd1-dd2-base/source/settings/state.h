#ifndef OPENTEC_BASE_SETTINGS_STATE_H
#define OPENTEC_BASE_SETTINGS_STATE_H

#include "profile/bank.h"
#include "shifter/h_pattern.h"
#include "wheel/position.h"

typedef struct {
    TuningProfileBank tuning_profiles;
    WheelPositionReference wheel_position;
    HPatternSettings h_pattern_shifter;
} BaseSettings;

void base_settings_defaults(BaseSettings *settings);

#endif
