#ifndef OPENTEC_BASE_SYSTEM_CONTROL_STATE_H
#define OPENTEC_BASE_SYSTEM_CONTROL_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "remote_tuning/response.h"
#include "system/torque_transition.h"

typedef struct {
    uint16_t status_code;
    RemoteTuningResponse wheel_response;
    uint8_t active_event_code;
    uint8_t operating_status;
    bool operating_feature_enabled;
    bool display_state_pending;
} SystemControlState;

void system_control_state_init(SystemControlState *state);
void system_control_state_set_status(SystemControlState *state, uint8_t wheel_mode, uint16_t code);
void system_control_state_set_active_event(SystemControlState *state, uint8_t code);
bool system_control_state_take_display_state(SystemControlState *state, uint8_t *code);
bool system_control_state_take_status(SystemControlState *state, uint16_t *code);
bool system_control_state_take_wheel_response(SystemControlState *state,
                                              RemoteTuningResponse *response);
void system_control_state_set_operating_feature(SystemControlState *state, bool enabled);
void system_control_state_set_operating_status(SystemControlState *state, uint8_t wheel_mode,
                                               bool enabled);
void system_control_state_apply_torque_transition(SystemControlState *state, uint8_t wheel_mode,
                                                  uint8_t setup_page,
                                                  const SystemTorqueTransitionAction *action);

#endif
