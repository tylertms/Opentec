#include "system/torque_transition.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    SYSTEM_EVENT_TORQUE_DISABLED = 0x0d,
    SYSTEM_EVENT_IDLE = 0x11,
    SYSTEM_EVENT_TORQUE_ENABLED = 0x1b,
    SYSTEM_STATUS_IDLE = 0x1e,
    SYSTEM_STATUS_TORQUE_DISABLED = 0x2b,
    MOTOR_CONTROL_RESUME = 0x10,
    WHEEL_MODE_EXTENDED = 0x1c,
};

/**
 * @brief Initializes the applied torque disable state.
 *
 * Starts with torque enabled and ready to accept a disable request.
 *
 * @param[out] transition Torque transition state to initialize.
 */
void system_torque_transition_init(SystemTorqueTransition *transition) {
    *transition = (SystemTorqueTransition){0};
}

/**
 * @brief Builds a queued system transition for a torque disable-state change.
 *
 * Leaves the request pending while the event slot is occupied. Disabling torque emits event 0x0d.
 * Restoring torque queues event 0x1b and selects active event 0x11. Wheel mode 0x1c additionally
 * changes its feature state and selects status 0x2b while disabled; when restoring torque it
 * selects status 0x1e for operating status zero or motor-control state 0x10 otherwise.
 *
 * @param[in,out] transition Last torque disable state accepted by the system event path.
 * @param[in] disable_requested Requested torque disable state.
 * @param[in] event_slot_available True when the single pending-event slot can accept a code.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] operating_status Current operating-mode status byte.
 * @param[out] action Transition codes and optional mode-specific updates.
 * @return True when a new transition was accepted and action was populated.
 */
bool system_torque_transition_update(SystemTorqueTransition *transition, bool disable_requested,
                                     bool event_slot_available, uint8_t wheel_mode,
                                     uint8_t operating_status,
                                     SystemTorqueTransitionAction *action) {
    *action = (SystemTorqueTransitionAction){0};
    if (disable_requested == transition->applied_disabled || !event_slot_available) {
        return false;
    }

    action->pending_event_code =
        disable_requested ? SYSTEM_EVENT_TORQUE_DISABLED : SYSTEM_EVENT_TORQUE_ENABLED;
    action->active_event_code =
        disable_requested ? SYSTEM_EVENT_TORQUE_DISABLED : SYSTEM_EVENT_IDLE;

    if (wheel_mode == WHEEL_MODE_EXTENDED) {
        action->feature_update = true;
        action->feature_enabled = disable_requested;
        if (disable_requested) {
            action->status_update = true;
            action->status_code = SYSTEM_STATUS_TORQUE_DISABLED;
        } else if (operating_status == 0) {
            action->status_update = true;
            action->status_code = SYSTEM_STATUS_IDLE;
        } else {
            action->motor_control_update = true;
            action->motor_control_state = MOTOR_CONTROL_RESUME;
        }
    }

    transition->applied_disabled = disable_requested;
    return true;
}
