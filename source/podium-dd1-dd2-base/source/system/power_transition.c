#include "system/power_transition.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    SYSTEM_EVENT_POWER_ON = 0x0d,
    SYSTEM_EVENT_IDLE = 0x11,
    SYSTEM_EVENT_POWER_OFF = 0x1b,
    SYSTEM_STATUS_IDLE = 0x1e,
    SYSTEM_STATUS_POWER_ON = 0x2b,
    MOTOR_CONTROL_POWER_OFF = 0x10,
    WHEEL_MODE_EXTENDED = 0x1c,
};

/**
 * @brief Initializes the applied system power state.
 *
 * Starts with the system transition path off and ready to accept an on request.
 *
 * @param[out] transition Power transition state to initialize.
 */
void system_power_transition_init(SystemPowerTransition *transition) {
    *transition = (SystemPowerTransition){0};
}

/**
 * @brief Builds a queued system transition for a requested power-state change.
 *
 * Leaves the request pending while the event slot is occupied. An accepted on transition emits
 * event 0x0d. An accepted off transition queues event 0x1b and selects active event 0x11. Wheel
 * mode 0x1c additionally changes its feature state and selects status 0x2b when enabling; when
 * disabling it selects status 0x1e for operating status zero or motor-control state 0x10 otherwise.
 *
 * @param[in,out] transition Last power state accepted by the system event path.
 * @param[in] requested_on Requested system power state.
 * @param[in] event_slot_available True when the single pending-event slot can accept a code.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] operating_status Current operating-mode status byte.
 * @param[out] action Transition codes and optional mode-specific updates.
 * @return True when a new transition was accepted and action was populated.
 */
bool system_power_transition_update(SystemPowerTransition *transition, bool requested_on,
                                    bool event_slot_available, uint8_t wheel_mode,
                                    uint8_t operating_status, SystemPowerTransitionAction *action) {
    *action = (SystemPowerTransitionAction){0};
    if (requested_on == transition->applied_on || !event_slot_available) {
        return false;
    }

    action->pending_event_code = requested_on ? SYSTEM_EVENT_POWER_ON : SYSTEM_EVENT_POWER_OFF;
    action->active_event_code = requested_on ? SYSTEM_EVENT_POWER_ON : SYSTEM_EVENT_IDLE;

    if (wheel_mode == WHEEL_MODE_EXTENDED) {
        action->feature_update = true;
        action->feature_enabled = requested_on;
        if (requested_on) {
            action->status_update = true;
            action->status_code = SYSTEM_STATUS_POWER_ON;
        } else if (operating_status == 0) {
            action->status_update = true;
            action->status_code = SYSTEM_STATUS_IDLE;
        } else {
            action->motor_control_update = true;
            action->motor_control_state = MOTOR_CONTROL_POWER_OFF;
        }
    }

    transition->applied_on = requested_on;
    return true;
}
