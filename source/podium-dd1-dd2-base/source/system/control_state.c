#include "system/control_state.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "system/torque_transition.h"

/**
 * @brief Internal status and wheel-mode values used by system-control state.
 *
 * These constants define status normalization and response selection for attached-wheel modes.
 */
enum {
    STATUS_NORMALIZATION_WHEEL_MODE = 0x18, /**< Wheel mode using normalized status values. */
    STATUS_NORMALIZATION_FIRST = 0x80,      /**< First low-byte status subject to normalization. */
    STATUS_NORMALIZATION_LAST = 0x8f,       /**< Last low-byte status subject to normalization. */
    STATUS_NORMALIZED = 0x13,               /**< Canonical normalized status value. */
    STATUS_NONE = 0xffff,                   /**< Internal marker for no pending status. */
    WHEEL_MODE_TRANSITION = 0x0e,           /**< Wheel mode using legacy operating responses. */
    WHEEL_MODE_EXTENDED = 0x1c,             /**< Wheel mode using extended operating responses. */
};

void system_control_state_init(SystemControlState *state) {
    *state = (SystemControlState){
        .status_code = STATUS_NONE,
    };
}

void system_control_state_set_status(SystemControlState *state, uint8_t wheel_mode, uint16_t code) {
    uint8_t low = (uint8_t)code;
    state->status_code = wheel_mode == STATUS_NORMALIZATION_WHEEL_MODE &&
                                 low >= STATUS_NORMALIZATION_FIRST &&
                                 low <= STATUS_NORMALIZATION_LAST
                             ? STATUS_NORMALIZED
                             : code;
}

void system_control_state_set_active_event(SystemControlState *state, uint8_t code) {
    state->active_event_code = code;
    state->display_state_pending = code != 0;
}

bool system_control_state_take_display_state(SystemControlState *state, uint8_t *code) {
    if (state == NULL || code == NULL || !state->display_state_pending) {
        return false;
    }
    *code = state->active_event_code;
    state->display_state_pending = false;
    return true;
}

bool system_control_state_take_status(SystemControlState *state, uint16_t *code) {
    if (state == NULL || code == NULL || state->status_code == STATUS_NONE) {
        return false;
    }
    *code = state->status_code;
    state->status_code = STATUS_NONE;
    return true;
}

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

void system_control_state_set_operating_feature(SystemControlState *state, bool enabled) {
    state->operating_feature_enabled = enabled;
}

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

void system_control_state_apply_torque_transition(SystemControlState *state, uint8_t wheel_mode,
                                                  uint8_t setup_page,
                                                  const SystemTorqueTransitionAction *action) {
    system_control_state_set_active_event(state, action->active_event_code);
    if (action->feature_update) {
        system_control_state_set_operating_feature(state, action->feature_enabled);
    }
    if (action->status_update) {
        system_control_state_set_status(state, wheel_mode, action->status_code);
    }
    if (wheel_mode == WHEEL_MODE_EXTENDED && action->next_page_response) {
        state->wheel_response = (RemoteTuningResponse){
            .link = REMOTE_TUNING_LINK_EXTENDED,
            .code = REMOTE_TUNING_RESPONSE_NEXT_SETUP_PAGE,
            .value = setup_page,
        };
    }
}
