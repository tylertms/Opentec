#ifndef OPENTEC_BASE_WHEEL_CAPABILITY_H
#define OPENTEC_BASE_WHEEL_CAPABILITY_H

#include <stdbool.h>
#include <stdint.h>

#include "profile/tuning.h"
#include "usb/operating_mode_command.h"

typedef struct {
    uint16_t report_flags;
    uint16_t capability_flags;
    uint8_t multi_position_override;
    bool calibration_available;
    bool tuning_menu_available;
    bool input_available;
} WheelCapabilityState;

void wheel_capability_init(WheelCapabilityState *state);
void wheel_capability_update(WheelCapabilityState *state, uint8_t wheel_mode, uint8_t report_mode,
                             uint8_t report_capabilities);
bool wheel_capability_input_available(const WheelCapabilityState *state, uint8_t wheel_mode);
bool wheel_capability_tuning_menu_available(const WheelCapabilityState *state, uint8_t wheel_mode);
bool wheel_capability_apply_multi_position_command(WheelCapabilityState *state,
                                                   const UsbOperatingModeCommand *command);
uint8_t wheel_capability_multi_position_mode(const WheelCapabilityState *state,
                                             TuningMultiPositionMode configured_mode,
                                             uint8_t wheel_mode, bool input_active);

#endif
