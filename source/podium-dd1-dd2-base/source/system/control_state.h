#ifndef OPENTEC_BASE_SYSTEM_CONTROL_STATE_H
#define OPENTEC_BASE_SYSTEM_CONTROL_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "system/torque_transition.h"

typedef struct {
    uint16_t status_code;
    uint16_t hid_configuration;
    uint16_t hid_response_flags;
    uint8_t active_event_code;
    uint8_t motor_control_state;
    uint8_t operating_status;
    uint8_t operating_transition_code;
    bool operating_feature_enabled;
} SystemControlState;

void system_control_state_init(SystemControlState *state);
void system_control_state_set_status(SystemControlState *state, uint8_t wheel_mode, uint16_t code);
void system_control_state_set_operating_feature(SystemControlState *state, bool enabled);
void system_control_state_set_motor_control(SystemControlState *state, uint8_t wheel_mode,
                                            uint8_t control_state);
void system_control_state_set_operating_status(SystemControlState *state, uint8_t wheel_mode,
                                               bool enabled);
void system_control_state_apply_torque_transition(SystemControlState *state, uint8_t wheel_mode,
                                                  const SystemTorqueTransitionAction *action);

#endif
