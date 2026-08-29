#ifndef OPENTEC_BASE_SYSTEM_POWER_TRANSITION_H
#define OPENTEC_BASE_SYSTEM_POWER_TRANSITION_H

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
} SystemPowerTransitionAction;

typedef struct {
    bool applied_on;
} SystemPowerTransition;

void system_power_transition_init(SystemPowerTransition *transition);
bool system_power_transition_update(SystemPowerTransition *transition, bool requested_on,
                                    bool event_slot_available, uint8_t wheel_mode,
                                    uint8_t operating_status, SystemPowerTransitionAction *action);

#endif
