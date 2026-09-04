#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_MOTION_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_MOTION_H

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/script_operand.h"

/**
 * @brief Previous state required to derive script motion values.
 *
 * The state keeps the prior motion-clock sample and the previous position and velocity used by
 * finite differences.
 */
typedef struct {
    uint32_t tick_snapshot;  /**< Motion-clock value at the previous update. */
    float previous_position; /**< Normalized position from the previous update. */
    float previous_velocity; /**< Derived velocity from the previous update. */
} ForceFeedbackScriptMotionState;

/**
 * @brief Update script motion values from wheel and live-input state.
 *
 * Uses motion value 0 as the input selector. When integration is enabled, a matching live-input
 * slot adds its floating-point duration to position and clamps the result to -1 through 1. If no
 * slot matches and the selector is the position status, position is sampled as wheel_position
 * divided by half_travel; other unmatched selectors retain position. Derives angle from the raw
 * encoded sensitivity code and its separate decoded extended range, then derives velocity and
 * acceleration from the motion clock. Stores position, angle, velocity, and acceleration in
 * motion values 4 through 7 and axes 0 through 3 respectively.
 *
 * @param[in,out] runtime Script motion values, rotation range, and axes to update.
 * @param[in] inputs Live integration inputs and selector statuses.
 * @param[in,out] state Previous position, velocity, and motion-clock state.
 * @param[in] motion_ticks Current motion-clock count.
 * @param[in] wheel_position Signed raw wheel-position sample.
 * @param[in] half_travel Positive raw wheel travel from center to either endpoint.
 * @param[in] integrate_inputs true to apply a matching live-input slot; false to use wheel input
 * or the retained position.
 * @pre runtime, inputs, and state point to valid objects.
 * @pre half_travel is nonzero when the selector selects wheel sampling.
 * @pre motion_ticks differs from state->tick_snapshot.
 */
void force_feedback_script_motion_update(ForceFeedbackScriptRuntime *runtime,
                                         const ForceFeedbackScriptInputs *inputs,
                                         ForceFeedbackScriptMotionState *state,
                                         uint32_t motion_ticks, int32_t wheel_position,
                                         uint32_t half_travel, bool integrate_inputs);

#endif
