#include "force_feedback/state.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Converts an unsigned effect position to the centered wheel-position scale.
 *
 * Maps zero to the negative position limit and 128 to the center. The multiply is divided by
 * 256 with truncation toward zero before the negative limit is applied.
 *
 * @param[in] position Unsigned position from the short force-feedback command.
 * @param[in] scale Positive and negative wheel-position limit.
 * @return Centered wheel position in the supplied scale.
 */
static int32_t center_position(uint8_t position, int32_t scale) {
    return (int32_t)((int64_t)scale * position * 2 / 256) - scale;
}

/**
 * @brief Initializes the host-controlled force-feedback state.
 *
 * Clears the 16 host effect slots and creates the active built-in position effect in slot 16.
 * The built-in effect is centered on both axes, uses axis mode 4 in both directions, opposes
 * motion in both directions, and starts at full strength.
 *
 * @param[out] state Force-feedback state to initialize.
 */
void force_feedback_state_init(ForceFeedbackState *state) {
    memset(state, 0, sizeof(*state));

    ForceFeedbackEffectState *position = &state->effects[FORCE_FEEDBACK_POSITION_EFFECT_SLOT];
    position->kind = FORCE_FEEDBACK_EFFECT_KIND_2;
    position->active = true;
    position->kind_2.axis_modes[0] = 4;
    position->kind_2.axis_modes[1] = 4;
    position->kind_2.directions[0] = -1;
    position->kind_2.directions[1] = -1;
    position->kind_2.strength = UINT16_MAX;
}

/**
 * @brief Applies one decoded short force-feedback command to the effect state.
 *
 * Replaces and activates configured host slots, deactivates cleared slots without deleting their
 * configuration, controls the built-in position effect, and updates both output gates. A primary
 * output-gate command also deactivates all 16 host-controlled effects.
 *
 * @param[in,out] state Force-feedback state to update.
 * @param[in] command Decoded short force-feedback command.
 * @param[in] position_scale Positive and negative wheel-position limit for kind-2 positions.
 * @return True when the command and effect slot are accepted.
 */
bool force_feedback_state_apply(ForceFeedbackState *state, const ForceFeedbackCommand *command,
                                int32_t position_scale) {
    if (state == NULL || command == NULL) {
        return false;
    }

    if (command->kind == FORCE_FEEDBACK_COMMAND_SET_PRIMARY_OUTPUT) {
        for (uint8_t slot = 0; slot < FORCE_FEEDBACK_EFFECT_SLOT_COUNT; ++slot) {
            state->effects[slot].active = false;
        }
        state->primary_output_disabled = command->output_disabled;
        return true;
    }

    if (command->kind == FORCE_FEEDBACK_COMMAND_SET_SECONDARY_OUTPUT) {
        state->secondary_output_disabled = command->output_disabled;
        return true;
    }

    if (command->kind == FORCE_FEEDBACK_COMMAND_ACTIVATE_POSITION_EFFECT) {
        state->effects[FORCE_FEEDBACK_POSITION_EFFECT_SLOT].active = true;
        return true;
    }

    if (command->kind == FORCE_FEEDBACK_COMMAND_CLEAR_POSITION_EFFECT) {
        state->effects[FORCE_FEEDBACK_POSITION_EFFECT_SLOT].active = false;
        return true;
    }

    if (command->slot >= FORCE_FEEDBACK_EFFECT_SLOT_COUNT) {
        return false;
    }

    ForceFeedbackEffectState *effect = &state->effects[command->slot];
    if (command->kind == FORCE_FEEDBACK_COMMAND_CLEAR_EFFECT) {
        effect->active = false;
        return true;
    }

    if (command->kind != FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_1 &&
        command->kind != FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_2 &&
        command->kind != FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_3) {
        return false;
    }

    *effect = (ForceFeedbackEffectState){0};
    effect->active = true;
    if (command->kind == FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_1) {
        effect->kind = FORCE_FEEDBACK_EFFECT_KIND_1;
        effect->kind_1.magnitude = command->magnitude;
        return true;
    }

    if (command->kind == FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_2) {
        effect->kind = FORCE_FEEDBACK_EFFECT_KIND_2;
        effect->kind_2.positions[0] = center_position(command->positions[0], position_scale);
        effect->kind_2.positions[1] = center_position(command->positions[1], position_scale);
        effect->kind_2.axis_modes[0] = command->axis_modes[0];
        effect->kind_2.axis_modes[1] = command->axis_modes[1];
        effect->kind_2.directions[0] = command->directions[0];
        effect->kind_2.directions[1] = command->directions[1];
        effect->kind_2.strength = command->strength;
        return true;
    }

    if (command->kind == FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_3) {
        effect->kind = FORCE_FEEDBACK_EFFECT_KIND_3;
        effect->kind_3.mode = command->mode;
        effect->kind_3.axis_mode = command->axis_modes[0];
        effect->kind_3.directions[0] = command->directions[0];
        effect->kind_3.directions[1] = command->directions[1];
        effect->kind_3.strength = command->strength;
        return true;
    }

    return false;
}

/**
 * @brief Rescales all kind-2 effect positions after the wheel-position scale changes.
 *
 * Visits the 16 host-controlled effects and built-in slot 16 regardless of active state. Each
 * stored position is multiplied by the new scale shifted right by seven, then divided by the old
 * scale shifted right by seven with truncation toward zero.
 *
 * @param[in,out] state Force-feedback state containing the positions to update.
 * @param[in] previous_scale Wheel-position scale used to configure the stored positions.
 * @param[in] current_scale Replacement wheel-position scale.
 * @return True when the state is accepted and the previous scaled divisor is nonzero.
 */
bool force_feedback_state_rescale_positions(ForceFeedbackState *state, int32_t previous_scale,
                                            int32_t current_scale) {
    if (state == NULL) {
        return false;
    }
    if (previous_scale == current_scale) {
        return true;
    }

    int32_t previous = previous_scale >> 7;
    if (previous == 0) {
        return false;
    }
    int32_t current = current_scale >> 7;

    for (uint8_t slot = 0; slot < FORCE_FEEDBACK_STATE_EFFECT_COUNT; ++slot) {
        ForceFeedbackEffectState *effect = &state->effects[slot];
        if (effect->kind != FORCE_FEEDBACK_EFFECT_KIND_2) {
            continue;
        }
        effect->kind_2.positions[0] =
            (int32_t)((int64_t)effect->kind_2.positions[0] * current / previous);
        effect->kind_2.positions[1] =
            (int32_t)((int64_t)effect->kind_2.positions[1] * current / previous);
    }
    return true;
}
