#include "motor/output_status.h"

#include <stdbool.h>
#include <stdint.h>

#include "motor/output_transport.h"

/**
 * @brief Replaces one flag in a motor output status byte.
 *
 * Sets the selected bit when the condition is true and clears it otherwise.
 *
 * @param[in] status Existing status byte.
 * @param[in] flag Single motor output status flag.
 * @param[in] enabled True when the flag must be set.
 * @return Status byte containing the requested flag value.
 */
static uint8_t set_flag(uint8_t status, uint8_t flag, bool enabled) {
    return enabled ? (uint8_t)(status | flag) : (uint8_t)(status & (uint8_t)~flag);
}

/**
 * @brief Initializes persistent motor output status.
 *
 * Clears every status flag before the first motor exchange.
 *
 * @param[out] status Motor output status state to initialize.
 */
void motor_output_status_init(MotorOutputStatus *status) { status->value = 0; }

/**
 * @brief Builds the status byte for the next motor output frame.
 *
 * Direct-force selection controls the remote-effects flag. Host output gates and full-torque
 * acknowledgement remain live in every interface mode. Xbox direct-force operation retains the
 * force-enable, override, transition, and disconnect gates from the preceding non-Xbox update;
 * other modes refresh those gates from the current runtime state.
 *
 * @param[in,out] status Persistent motor output status state.
 * @param[in] input Current force-output conditions.
 * @return Status byte for the next motor output frame.
 */
uint8_t motor_output_status_update(MotorOutputStatus *status, const MotorOutputStatusInput *input) {
    uint8_t value = set_flag(status->value, MOTOR_OUTPUT_STATUS_REMOTE_EFFECTS,
                             !input->direct_force && !input->xbox_mode);

    value = set_flag(value, MOTOR_OUTPUT_STATUS_PRIMARY_DISABLED, input->primary_disabled);
    value = set_flag(value, MOTOR_OUTPUT_STATUS_SECONDARY_DISABLED, input->secondary_disabled);
    value = set_flag(value, MOTOR_OUTPUT_STATUS_FULL_TORQUE, input->full_torque);

    if (!input->xbox_mode) {
        value = set_flag(value, MOTOR_OUTPUT_STATUS_ENABLED, input->force_enabled);
        value = set_flag(value, MOTOR_OUTPUT_STATUS_OVERRIDE_ACTIVE, input->override_active);
        value = set_flag(value, MOTOR_OUTPUT_STATUS_TRANSITION_ACTIVE, input->transition_active);
        value = set_flag(value, MOTOR_OUTPUT_STATUS_USB_DISCONNECTED, input->usb_disconnected);
    }

    status->value = value;
    return value;
}
