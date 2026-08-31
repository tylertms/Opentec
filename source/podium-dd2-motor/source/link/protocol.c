#include "link/protocol.h"

#include "force_feedback/command.h"

/** @brief Motor-link status bits and force-feedback service timing constants. */
enum {
    MOTOR_STATUS_REMOTE_EFFECTS = 1U << 0U,       /**< Remote-effects status bit. */
    MOTOR_STATUS_EFFECTS_ACTIVE = 1U << 1U,      /**< Effects-active status bit. */
    MOTOR_STATUS_REDUCED_CONTROLLER = 1U << 2U,   /**< Reduced-controller status bit. */
    MOTOR_STATUS_TRANSITION_ACTIVE = 1U << 3U,    /**< Transition-active status bit. */
    MOTOR_STATUS_TRANSITION_READY = 1U << 4U,     /**< Transition-ready status bit. */
    MOTOR_STATUS_SECONDARY_DISABLED = 1U << 5U,   /**< Secondary-force-disabled status bit. */
    MOTOR_STATUS_EFFECTS_SUSPENDED = 1U << 6U,    /**< Effects-suspended status bit. */
    MOTOR_STATUS_FULL_TORQUE = 1U << 7U,           /**< Full-torque status bit. */
    MOTOR_HOST_EFFECT_COUNT = 16U,                 /**< Number of host-controlled effect slots. */
    MOTOR_FORCE_FEEDBACK_START_DELAY = 10U,        /**< Initial local-effect service delay in ticks. */
    MOTOR_FORCE_RAMP_INTERVAL = 50U,               /**< Local-effect ramp interval in ticks. */
};

/**
 * @brief Tests whether a wrap-safe force-feedback deadline has passed.
 *
 * Signed tick subtraction preserves ordering across one unsigned counter wrap.
 *
 * @param[in] now Current motor service tick.
 * @param[in] deadline Scheduled force-feedback tick.
 * @return True only after the scheduled tick has passed.
 */
static bool motor_protocol_tick_passed(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) > 0;
}

void motor_protocol_initialize(MotorProtocolState *state, uint8_t normal_output_percent) {
    *state = (MotorProtocolState){
        .normal_output_percent = normal_output_percent,
        .next_force_feedback_tick = MOTOR_FORCE_FEEDBACK_START_DELAY,
        .next_force_ramp_tick = MOTOR_FORCE_RAMP_INTERVAL,
    };
    motor_force_feedback_engine_initialize(&state->force_feedback);
}

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

bool motor_protocol_frame_result_apply(MotorProtocolState *state, MotorLinkFrameResult result,
                                       const MotorLinkFrame *frame) {
    state->replay = result != MOTOR_LINK_FRAME_VALID;
    return result == MOTOR_LINK_FRAME_VALID && motor_protocol_frame_apply(state, frame);
}

bool motor_protocol_force_feedback_service(MotorProtocolState *state, uint32_t now,
                                           int32_t centered_position, int32_t position,
                                           int32_t velocity) {
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
    } else if (state->force_feedback.ramp_percent < 100U &&
               motor_protocol_tick_passed(now, state->next_force_ramp_tick)) {
        ++state->force_feedback.ramp_percent;
        state->next_force_ramp_tick = now + MOTOR_FORCE_RAMP_INTERVAL;
    }

    if (state->next_force_feedback_tick != now &&
        !motor_protocol_tick_passed(now, state->next_force_feedback_tick)) {
        return false;
    }
    state->next_force_feedback_tick = now + 1U;

    MotorForceFeedbackMix mix = motor_force_feedback_mix(
        &state->force_feedback, now, centered_position, state->center, position, velocity,
        (state->status & MOTOR_STATUS_SECONDARY_DISABLED) != 0U);
    state->live_drive = motor_drive_command_resolve(
        mix.primary.positive, mix.primary.magnitude, mix.secondary, state->normal_output_percent,
        (state->status & MOTOR_STATUS_FULL_TORQUE) != 0U,
        (state->status & MOTOR_STATUS_REDUCED_CONTROLLER) != 0U,
        (state->status & MOTOR_STATUS_SECONDARY_DISABLED) != 0U);
    state->live_drive_updated = true;
    return true;
}
