#ifndef OPENTEC_BASE_SYSTEM_TORQUE_TRANSITION_H
#define OPENTEC_BASE_SYSTEM_TORQUE_TRANSITION_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t pending_event_code;
    uint8_t active_event_code;
    uint8_t status_code;
    uint8_t motor_control_state;
    bool status_update;
    bool feature_update;
    bool feature_enabled;
    bool motor_control_update;
} SystemTorqueTransitionAction;

typedef struct {
    bool applied_disabled;
} SystemTorqueTransition;

void system_torque_transition_init(SystemTorqueTransition *transition);
bool system_torque_transition_update(SystemTorqueTransition *transition, bool disable_requested,
                                     bool event_slot_available, uint8_t wheel_mode,
                                     uint8_t operating_status,
                                     SystemTorqueTransitionAction *action);

#endif
