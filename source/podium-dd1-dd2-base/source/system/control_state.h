#ifndef OPENTEC_BASE_SYSTEM_CONTROL_STATE_H
#define OPENTEC_BASE_SYSTEM_CONTROL_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "remote_tuning/response.h"
#include "system/torque_transition.h"

/**
 * @brief Shared status, event, operating-mode, and wheel-response state.
 *
 * The state stores values produced by base-side system policy until the corresponding wheel or
 * display service consumes them.
 */
typedef struct {
    uint16_t status_code; /**< Pending 16-bit system status code, or the internal idle marker. */
    RemoteTuningResponse wheel_response; /**< Pending system-owned wheel response. */
    uint8_t active_event_code;           /**< Current system event code, or zero when cleared. */
    uint8_t operating_status;            /**< Canonical operating status, where zero is inactive. */
    bool operating_feature_enabled;      /**< Whether the host operating-mode feature is enabled. */
    bool display_state_pending; /**< Whether active_event_code awaits display consumption. */
} SystemControlState;

/**
 * @brief Initializes shared system-control state.
 *
 * Clears pending status and response values, resets the active event, and disables the
 * operating-mode feature.
 *
 * @param[out] state System-control state to initialize.
 */
void system_control_state_init(SystemControlState *state);

/**
 * @brief Stores the current system status code.
 *
 * Normalizes status codes in the attached wheel mode that uses the shared 0x13 status value and
 * retains all other codes unchanged.
 *
 * @param[in,out] state System-control state receiving the status.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] code Requested 16-bit status code.
 */
void system_control_state_set_status(SystemControlState *state, uint8_t wheel_mode, uint16_t code);

/**
 * @brief Stores the active system event code.
 *
 * Retains the accepted event for system consumers and marks its display state pending when the
 * code is nonzero.
 *
 * @param[in,out] state System-control state receiving the event.
 * @param[in] code Accepted system event code, or zero to clear the display-pending flag.
 */
void system_control_state_set_active_event(SystemControlState *state, uint8_t code);

/**
 * @brief Takes the pending attached-wheel display state.
 *
 * Returns the active nonzero event once and clears only its display-pending flag, leaving the
 * active event available to other consumers.
 *
 * @param[in,out] state System-control state that owns the pending display state.
 * @param[out] code Destination for the pending display state.
 * @return True when a pending display state was returned; otherwise false.
 */
bool system_control_state_take_display_state(SystemControlState *state, uint8_t *code);

/**
 * @brief Takes the pending attached-wheel status code.
 *
 * Returns the current status once and restores the internal idle marker after a successful take.
 *
 * @param[in,out] state System-control state that owns the pending code.
 * @param[out] code Destination for the pending 16-bit code.
 * @return True when a pending code was returned; otherwise false.
 */
bool system_control_state_take_status(SystemControlState *state, uint16_t *code);

/**
 * @brief Takes the pending system-owned attached-wheel response.
 *
 * Copies one pending semantic response and clears it; responses owned by the USB remote-tuning
 * service are not represented here.
 *
 * @param[in,out] state System-control state that owns the pending response.
 * @param[out] response Destination for the pending response.
 * @return True when a pending response was returned; otherwise false.
 */
bool system_control_state_take_wheel_response(SystemControlState *state,
                                              RemoteTuningResponse *response);

/**
 * @brief Applies the operating-mode feature enable state.
 *
 * Stores the feature state used by torque-transition and operating-mode policy.
 *
 * @param[in,out] state System-control state receiving the feature state.
 * @param[in] enabled True to enable the operating-mode feature.
 */
void system_control_state_set_operating_feature(SystemControlState *state, bool enabled);

/**
 * @brief Applies an operating-status change from the host.
 *
 * Stores a canonical zero-or-one status and retains the matching inactive or active response for
 * wheel modes that expose remote-tuning control. Extended-wheel mode also clears the operating-
 * mode feature before retaining its response.
 *
 * @param[in,out] state System-control state receiving the operating-status change.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] enabled True to enter the operating state.
 */
void system_control_state_set_operating_status(SystemControlState *state, uint8_t wheel_mode,
                                               bool enabled);

/**
 * @brief Applies an accepted torque transition to shared system-control state.
 *
 * Stores the active transition event and applies the status, feature, and next-page response
 * changes selected by the transition policy. An extended active restore retains the current page
 * as the response value while using semantic response code 0x10.
 *
 * @param[in,out] state System-control state receiving the transition.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] setup_page Current remote-tuning setup page carried by response code 0x10.
 * @param[in] action Accepted torque transition action.
 */
void system_control_state_apply_torque_transition(SystemControlState *state, uint8_t wheel_mode,
                                                  uint8_t setup_page,
                                                  const SystemTorqueTransitionAction *action);

#endif
