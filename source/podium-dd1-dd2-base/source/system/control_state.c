#include "system/control_state.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "system/torque_transition.h"

enum {
    STATUS_NORMALIZATION_WHEEL_MODE = 0x18,
    STATUS_NORMALIZATION_FIRST = 0x80,
    STATUS_NORMALIZATION_LAST = 0x8f,
    STATUS_NORMALIZED = 0x13,
    STATUS_NONE = 0xffff,
    WHEEL_MODE_TRANSITION = 0x0e,
    WHEEL_MODE_EXTENDED = 0x1c,
};

/**
 * @brief Initializes shared system-control state.
 *
 * Starts with no pending attached-wheel status or response, an idle event code, and the
 * operating-mode feature disabled.
 *
 * @param[out] state System-control state to initialize.
 */
void system_control_state_init(SystemControlState *state) {
    *state = (SystemControlState){
        .status_code = STATUS_NONE,
    };
}

/**
 * @brief Stores the current system status code.
 *
 * Wheel mode 0x18 maps codes whose low byte is 0x80 through 0x8f to status 0x13. Other codes and
 * wheel modes retain the complete 16-bit value.
 *
 * @param[in,out] state System-control state receiving the status.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] code Requested 16-bit status code.
 */
void system_control_state_set_status(SystemControlState *state, uint8_t wheel_mode, uint16_t code) {
    uint8_t low = (uint8_t)code;
    state->status_code = wheel_mode == STATUS_NORMALIZATION_WHEEL_MODE &&
                                 low >= STATUS_NORMALIZATION_FIRST &&
                                 low <= STATUS_NORMALIZATION_LAST
                             ? STATUS_NORMALIZED
                             : code;
}

/**
 * @brief Takes the pending attached-wheel status code.
 *
 * Returns the current code once and replaces it with the idle 0xFFFF marker used by the
 * attached-wheel response builder.
 *
 * @param[in,out] state System-control state that owns the pending code.
 * @param[out] code Destination for the pending 16-bit code.
 * @return True when a pending code was returned; otherwise false.
 */
bool system_control_state_take_status(SystemControlState *state, uint16_t *code) {
    if (state == NULL || code == NULL || state->status_code == STATUS_NONE) {
        return false;
    }
    *code = state->status_code;
    state->status_code = STATUS_NONE;
    return true;
}

/**
 * @brief Takes the pending system-owned attached-wheel response.
 *
 * Returns one semantic extended remote-tuning response and clears the retained response. Host
 * responses remain under the ownership of the USB remote-tuning service.
 *
 * @param[in,out] state System-control state that owns the pending response.
 * @param[out] response Destination for the pending response.
 * @return True when a pending response was returned; otherwise false.
 */
bool system_control_state_take_wheel_response(SystemControlState *state,
                                              RemoteTuningResponse *response) {
    if (state == NULL || response == NULL ||
        state->wheel_response.code == REMOTE_TUNING_RESPONSE_NONE) {
        return false;
    }
    *response = state->wheel_response;
    state->wheel_response = (RemoteTuningResponse){0};
    return true;
}

/**
 * @brief Applies the operating-mode feature enable state.
 *
 * Retains the state used by torque-transition and operating-mode policy.
 *
 * @param[in,out] state System-control state receiving the feature state.
 * @param[in] enabled True to enable the operating-mode feature.
 */
void system_control_state_set_operating_feature(SystemControlState *state, bool enabled) {
    state->operating_feature_enabled = enabled;
}

/**
 * @brief Applies an operating-status change from the host.
 *
 * Stores a canonical zero-or-one status. Remote-tuning wheel modes queue the matching inactive or
 * active response on their legacy or extended link.
 *
 * @param[in,out] state System-control state receiving the operating-status change.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] enabled True to enter the operating state.
 */
void system_control_state_set_operating_status(SystemControlState *state, uint8_t wheel_mode,
                                               bool enabled) {
    state->operating_status = enabled ? 1 : 0;
    if (wheel_mode == WHEEL_MODE_TRANSITION) {
        state->wheel_response = (RemoteTuningResponse){
            .link = REMOTE_TUNING_LINK_LEGACY,
            .code = enabled ? REMOTE_TUNING_RESPONSE_ACTIVE : REMOTE_TUNING_RESPONSE_INACTIVE,
        };
    } else if (wheel_mode == WHEEL_MODE_EXTENDED) {
        state->operating_feature_enabled = false;
        state->wheel_response = (RemoteTuningResponse){
            .link = REMOTE_TUNING_LINK_EXTENDED,
            .code = enabled ? REMOTE_TUNING_RESPONSE_ACTIVE : REMOTE_TUNING_RESPONSE_INACTIVE,
        };
    }
}

/**
 * @brief Applies an accepted torque transition to shared system-control state.
 *
 * Publishes the active event and applies the status and feature changes marked by the transition.
 * A setup response carries the current page selected by the remote-tuning owner.
 *
 * @param[in,out] state System-control state receiving the transition.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] setup_page Current remote-tuning setup page.
 * @param[in] action Accepted torque transition action.
 */
void system_control_state_apply_torque_transition(SystemControlState *state, uint8_t wheel_mode,
                                                  uint8_t setup_page,
                                                  const SystemTorqueTransitionAction *action) {
    state->active_event_code = action->active_event_code;
    if (action->feature_update) {
        system_control_state_set_operating_feature(state, action->feature_enabled);
    }
    if (action->status_update) {
        system_control_state_set_status(state, wheel_mode, action->status_code);
    }
    if (wheel_mode == WHEEL_MODE_EXTENDED && action->setup_response) {
        state->wheel_response = (RemoteTuningResponse){
            .link = REMOTE_TUNING_LINK_EXTENDED,
            .code = REMOTE_TUNING_RESPONSE_SETUP,
            .value = setup_page,
        };
    }
}
