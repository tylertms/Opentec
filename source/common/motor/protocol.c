#include "common/motor/protocol.h"

#include "common/motor/force_feedback_command.h"

enum {
    MOTOR_STATUS_REMOTE_EFFECTS = 1U << 0U,
    MOTOR_STATUS_REDUCED_CONTROLLER = 1U << 2U,
    MOTOR_STATUS_SECONDARY_DISABLED = 1U << 5U,
    MOTOR_STATUS_FULL_TORQUE = 1U << 7U,
};

/**
 * @brief Initializes the official motor-link status, drive, and force-feedback state.
 * @param state Motor protocol state to initialize.
 * @param normal_output_percent Product-specific output scale outside full-torque mode.
 */
void motor_protocol_initialize(MotorProtocolState *state, uint8_t normal_output_percent) {
    *state = (MotorProtocolState){
        .normal_output_percent = normal_output_percent,
    };
    motor_force_feedback_engine_initialize(&state->force_feedback);
}

/**
 * @brief Applies one decoded official motor-link force or status frame.
 * @param state Persistent motor protocol state.
 * @param frame Validated motor-link frame.
 * @return True when the frame type is supported.
 */
bool motor_protocol_frame_apply(MotorProtocolState *state, const MotorLinkFrame *frame) {
    MotorLinkForceCommand force;
    if (motor_link_force_command_decode(frame, &force)) {
        state->center = force.center;
        if ((state->status & MOTOR_STATUS_REMOTE_EFFECTS) == 0U) {
            state->live_drive = motor_drive_command_resolve(
                force.positive, force.primary, force.secondary, state->normal_output_percent,
                (state->status & MOTOR_STATUS_FULL_TORQUE) != 0U,
                (state->status & MOTOR_STATUS_REDUCED_CONTROLLER) != 0U,
                (state->status & MOTOR_STATUS_SECONDARY_DISABLED) != 0U);
            state->live_drive_updated = true;
        }
        return true;
    }

    MotorLinkStatusCommand status;
    if (motor_link_status_command_decode(frame, &status)) {
        state->status = status.status;
        (void)motor_force_feedback_command_apply(&state->force_feedback, status.command);
        return true;
    }
    return false;
}
