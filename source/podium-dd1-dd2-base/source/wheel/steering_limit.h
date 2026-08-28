#ifndef OPENTEC_BASE_WHEEL_STEERING_LIMIT_H
#define OPENTEC_BASE_WHEEL_STEERING_LIMIT_H

#include <stdbool.h>
#include <stdint.h>

#include "profile/bank.h"
#include "usb/operating_mode_command.h"

enum { WHEEL_STEERING_LIMIT_DEFAULT_PERCENT = 100 };

typedef struct {
    uint8_t percent[TUNING_PROFILE_SLOT_COUNT];
} WheelSteeringLimits;

typedef struct {
    uint8_t percent;
    bool reset_all;
} WheelSteeringLimitCommand;

typedef enum {
    WHEEL_STEERING_LIMIT_UNCHANGED,
    WHEEL_STEERING_LIMIT_CHANGED,
} WheelSteeringLimitResult;

void wheel_steering_limits_defaults(WheelSteeringLimits *limits);
bool wheel_steering_limit_command_decode(const UsbOperatingModeCommand *source,
                                         WheelSteeringLimitCommand *command);
WheelSteeringLimitResult wheel_steering_limits_apply(WheelSteeringLimits *limits,
                                                     uint8_t active_profile,
                                                     const WheelSteeringLimitCommand *command);
uint8_t wheel_steering_limits_active(const WheelSteeringLimits *limits, uint8_t active_profile);

#endif
