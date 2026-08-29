#include "motor/protocol.h"

#include "motor/force_feedback_command.h"

enum {
    MOTOR_STATUS_REMOTE_EFFECTS = 1U << 0U,
    MOTOR_STATUS_EFFECTS_ACTIVE = 1U << 1U,
    MOTOR_STATUS_REDUCED_CONTROLLER = 1U << 2U,
    MOTOR_STATUS_TRANSITION_ACTIVE = 1U << 3U,
    MOTOR_STATUS_TRANSITION_READY = 1U << 4U,
    MOTOR_STATUS_SECONDARY_DISABLED = 1U << 5U,
    MOTOR_STATUS_EFFECTS_SUSPENDED = 1U << 6U,
    MOTOR_STATUS_FULL_TORQUE = 1U << 7U,
    MOTOR_HOST_EFFECT_COUNT = 16U,
    MOTOR_FORCE_FEEDBACK_START_DELAY = 10U,
    MOTOR_FORCE_RAMP_INTERVAL = 50U,
};

/**
 * @brief Initializes the official motor-link status, drive, and force-feedback state.
 *
 * Product configuration supplies the normal output scale while the shared effect engine installs
 * the official position, damper, filter, and ramp defaults.
 *
 * @param state Motor protocol state to initialize.
 * @param normal_output_percent Product-specific output scale outside full-torque mode.
 */
void motor_protocol_initialize(MotorProtocolState *state, uint8_t normal_output_percent) {
    *state = (MotorProtocolState){
        .normal_output_percent = normal_output_percent,
        .next_force_feedback_tick = MOTOR_FORCE_FEEDBACK_START_DELAY,
        .next_force_ramp_tick = MOTOR_FORCE_RAMP_INTERVAL,
    };
    motor_force_feedback_engine_initialize(&state->force_feedback);
}

/**
 * @brief Applies one decoded official motor-link force or status frame.
 *
 * Live-force frames update the center and directly resolve drive output unless status selects the
 * local effect engine. Status frames publish their status byte and apply one effect command.
 *
 * @param state Persistent motor protocol state.
 * @param frame Validated motor-link frame.
 * @return False for an unknown frame type or rejected effect configuration.
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
        return motor_force_feedback_command_apply(&state->force_feedback, status.command);
    }
    return false;
}

/**
 * @brief Applies the official motor-link validation result and replay state.
 *
 * Boundary or checksum failures arm bit seven in the next position response. Every valid frame
 * clears that replay indication before its decoded payload is applied.
 *
 * @param state Persistent motor protocol and replay state.
 * @param result Boundary and checksum validation result.
 * @param frame Decoded frame supplied when validation succeeded.
 * @return True when the frame was valid and its type was supported.
 */
bool motor_protocol_frame_result_apply(MotorProtocolState *state, MotorLinkFrameResult result,
                                       const MotorLinkFrame *frame) {
    state->replay = result != MOTOR_LINK_FRAME_VALID;
    return result == MOTOR_LINK_FRAME_VALID && motor_protocol_frame_apply(state, frame);
}

/**
 * @brief Services the official local force-feedback path.
 *
 * Status gates disable the sixteen host-controlled slots and reset the recovery ramp where
 * required. One centered-position mix is converted to product-scaled drive output per service
 * tick while local effects remain selected.
 *
 * @param state Persistent motor protocol and force-feedback state.
 * @param now Current motor service tick.
 * @param centered_position Current encoder position relative to the commanded center.
 * @param velocity Current signed filtered encoder velocity.
 * @return True when a new live drive command was produced.
 */
bool motor_protocol_force_feedback_service(MotorProtocolState *state, uint32_t now,
                                           int32_t centered_position, int32_t velocity) {
    if ((state->status & MOTOR_STATUS_REMOTE_EFFECTS) == 0U) {
        return false;
    }

    bool transition_blocked = (state->status & MOTOR_STATUS_TRANSITION_ACTIVE) != 0U &&
                              (state->status & MOTOR_STATUS_TRANSITION_READY) == 0U;
    bool effects_blocked = (state->status & MOTOR_STATUS_REDUCED_CONTROLLER) != 0U ||
                           (state->status & MOTOR_STATUS_EFFECTS_ACTIVE) == 0U ||
                           (state->status & MOTOR_STATUS_EFFECTS_SUSPENDED) != 0U ||
                           transition_blocked;
    if (effects_blocked) {
        if ((state->status & MOTOR_STATUS_REDUCED_CONTROLLER) != 0U || transition_blocked) {
            state->force_feedback.ramp_percent = 0U;
        }
        for (uint8_t slot = 0U; slot < MOTOR_HOST_EFFECT_COUNT; ++slot) {
            (void)motor_force_feedback_effect_disable(&state->force_feedback, slot);
        }
    } else if (state->force_feedback.ramp_percent < 100U && state->next_force_ramp_tick < now) {
        ++state->force_feedback.ramp_percent;
        state->next_force_ramp_tick = now + MOTOR_FORCE_RAMP_INTERVAL;
    }

    if (state->next_force_feedback_tick > now) {
        return false;
    }
    state->next_force_feedback_tick = now + 1U;

    MotorForceFeedbackMix mix =
        motor_force_feedback_mix(&state->force_feedback, now, 0, centered_position, velocity,
                                 (state->status & MOTOR_STATUS_SECONDARY_DISABLED) != 0U);
    state->live_drive = motor_drive_command_resolve(
        mix.primary.positive, mix.primary.magnitude, mix.secondary, state->normal_output_percent,
        (state->status & MOTOR_STATUS_FULL_TORQUE) != 0U,
        (state->status & MOTOR_STATUS_REDUCED_CONTROLLER) != 0U,
        (state->status & MOTOR_STATUS_SECONDARY_DISABLED) != 0U);
    state->live_drive_updated = true;
    return true;
}
