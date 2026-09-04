#ifndef OPENTEC_BASE_SYSTEM_TORQUE_TRANSITION_H
#define OPENTEC_BASE_SYSTEM_TORQUE_TRANSITION_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Effects selected for an accepted torque disable-state transition.
 *
 * The action separates event, status, feature, and next-page response updates so each owning
 * service can apply only the changes relevant to its protocol.
 */
typedef struct {
    uint8_t pending_event_code; /**< Event code to queue for presentation. */
    uint8_t active_event_code;  /**< Event code to retain as the active system event. */
    uint8_t status_code;        /**< Status code to publish when status_update is true. */
    bool status_update;         /**< Whether status_code must be published. */
    bool feature_update;     /**< Whether feature_enabled must replace the current feature state. */
    bool feature_enabled;    /**< New operating-mode feature state when feature_update is true. */
    bool next_page_response; /**< Whether an extended active restore needs response 0x10. */
} SystemTorqueTransitionAction;

/**
 * @brief Last applied torque disable state.
 *
 * The transition controller compares each request with this state before producing a new action.
 */
typedef struct {
    bool applied_disabled; /**< Whether the last accepted transition disabled torque. */
} SystemTorqueTransition;

/**
 * @brief Initializes the applied torque disable state.
 *
 * Starts with torque enabled and no pending transition.
 *
 * @param[out] transition Torque transition state to initialize.
 */
void system_torque_transition_init(SystemTorqueTransition *transition);

/**
 * @brief Builds a queued system transition for a torque disable-state change.
 *
 * Produces event, status, feature, and next-page response effects only when the requested state
 * differs from the last accepted state and the event path can accept a new event. The state
 * remains unchanged until system_torque_transition_accept() confirms that the event was queued.
 *
 * @param[in] transition Last torque disable state accepted by the system event path.
 * @param[in] disable_requested Requested torque disable state.
 * @param[in] event_slot_available True when the event path can accept a code.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] operating_status Current operating-mode status byte.
 * @param[out] action Transition codes and optional mode-specific updates.
 * @return True when a new transition action was populated.
 */
bool system_torque_transition_update(const SystemTorqueTransition *transition,
                                     bool disable_requested, bool event_slot_available,
                                     uint8_t wheel_mode, uint8_t operating_status,
                                     SystemTorqueTransitionAction *action);

/**
 * @brief Commits a torque transition after its event was queued.
 *
 * Records the requested torque state only after the owning event path accepts the generated event,
 * allowing a failed queue attempt to be retried without losing the override request.
 *
 * @param[in,out] transition Torque transition state to update.
 * @param[in] disable_requested Torque disable state accepted by the event path.
 */
void system_torque_transition_accept(SystemTorqueTransition *transition, bool disable_requested);

#endif
