#include "system/torque_transition.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Internal torque transition event, status, and wheel-mode values.
 *
 * These values are translated into SystemTorqueTransitionAction fields for the integration layer.
 */
enum {
    SYSTEM_EVENT_TORQUE_DISABLED = 0x0d,  /**< Event code published when torque is disabled. */
    SYSTEM_EVENT_IDLE = 0x11,             /**< Event code published when torque is restored. */
    SYSTEM_EVENT_TORQUE_ENABLED = 0x1b,   /**< Event code queued when torque is restored. */
    SYSTEM_STATUS_IDLE = 0x1e,            /**< Extended-wheel status for inactive operation. */
    SYSTEM_STATUS_TORQUE_DISABLED = 0x2b, /**< Extended-wheel status for disabled torque. */
    WHEEL_MODE_EXTENDED = 0x1c,           /**< Wheel mode receiving extended transition updates. */
};

void system_torque_transition_init(SystemTorqueTransition *transition) {
    *transition = (SystemTorqueTransition){0};
}

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
            action->setup_response = true;
        }
    }

    transition->applied_disabled = disable_requested;
    return true;
}
