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

typedef struct {
    TuningProfileBank tuning_profiles;
    WheelPositionReference wheel_position;
    HPatternSettings h_pattern_shifter;
    AuxiliaryAxisSettings auxiliary_axis;
    WheelSteeringLimits steering_limits;
    SecurityCodeSettings security_code;
    uint8_t wheel_auxiliary_option;
} BaseSettings;

void base_settings_defaults(BaseSettings *settings);

#endif
