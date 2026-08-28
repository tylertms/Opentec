#ifndef OPENTEC_BASE_WHEEL_CAPABILITY_H
#define OPENTEC_BASE_WHEEL_CAPABILITY_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t report_flags;
    uint16_t capability_flags;
    bool calibration_available;
    bool tuning_menu_available;
} WheelCapabilityState;

void wheel_capability_update(WheelCapabilityState *state, uint8_t wheel_mode, uint8_t report_mode,
                             uint8_t report_capabilities);

#endif
