#include "system/control_state.h"

#include <stdbool.h>
#include <stdint.h>

#include "system/torque_transition.h"

enum {
    HID_CONFIGURATION_BASELINE = 0x20,
    HID_CONFIGURATION_OPERATING_FEATURE = 0x0001,
    HID_CONFIGURATION_COMMAND_RESET_MASK = 0xff83,
    HID_RESPONSE_REFRESH = 0x0002,
    STATUS_NORMALIZATION_WHEEL_MODE = 0x18,
    STATUS_NORMALIZATION_FIRST = 0x80,
    STATUS_NORMALIZATION_LAST = 0x8f,
    STATUS_NORMALIZED = 0x13,
    WHEEL_MODE_EXTENDED = 0x1c,
};

/**
 * @brief Initializes shared system-control state.
 *
 * Starts from status, event, and motor-control state zero with the baseline HID capability bit
 * enabled and the operating-mode feature disabled.
 *
 * @param[out] state System-control state to initialize.
 */
void system_control_state_init(SystemControlState *state) {
    *state = (SystemControlState){.hid_configuration = HID_CONFIGURATION_BASELINE};
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
 * @brief Applies the operating-mode feature enable state.
 *
 * Mirrors the state into HID configuration bit zero while preserving every other capability bit.
 *
 * @param[in,out] state System-control state receiving the feature state.
 * @param[in] enabled True to enable the operating-mode feature.
 */
void system_control_state_set_operating_feature(SystemControlState *state, bool enabled) {
    state->operating_feature_enabled = enabled;
    state->hid_configuration =
        (uint16_t)((state->hid_configuration & ~HID_CONFIGURATION_OPERATING_FEATURE) |
                   (enabled ? HID_CONFIGURATION_OPERATING_FEATURE : 0));
}

/**
 * @brief Applies a motor-link control state change.
 *
 * Stores the control state, clears transient HID command bits, requests a refreshed response, and
 * clears the operating-mode feature bit for wheel mode 0x1c.
 *
 * @param[in,out] state System-control state receiving the motor-control change.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] control_state Requested motor-link control state.
 */
void system_control_state_set_motor_control(SystemControlState *state, uint8_t wheel_mode,
                                            uint8_t control_state) {
    state->motor_control_state = control_state;
    state->hid_configuration &= HID_CONFIGURATION_COMMAND_RESET_MASK;
    state->hid_response_flags |= HID_RESPONSE_REFRESH;
    if (wheel_mode == WHEEL_MODE_EXTENDED) {
        state->hid_configuration &= (uint16_t)~HID_CONFIGURATION_OPERATING_FEATURE;
        state->operating_feature_enabled = false;
    }
}

/**
 * @brief Applies an accepted torque transition to shared system-control state.
 *
 * Publishes the active event and applies only the status, feature, and motor-control changes marked
 * by the transition action.
 *
 * @param[in,out] state System-control state receiving the transition.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] action Accepted torque transition action.
 */
void system_control_state_apply_torque_transition(SystemControlState *state, uint8_t wheel_mode,
                                                  const SystemTorqueTransitionAction *action) {
    state->active_event_code = action->active_event_code;
    if (action->feature_update) {
        system_control_state_set_operating_feature(state, action->feature_enabled);
    }
    if (action->status_update) {
        system_control_state_set_status(state, wheel_mode, action->status_code);
    }
    if (action->motor_control_update) {
        system_control_state_set_motor_control(state, wheel_mode, action->motor_control_state);
    }
}
