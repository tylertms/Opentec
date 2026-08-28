#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_MOTION_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_MOTION_H

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/script_operand.h"

typedef struct {
    uint32_t tick_snapshot;
    float previous_position;
    float previous_velocity;
} ForceFeedbackScriptMotionState;

void force_feedback_script_motion_update(ForceFeedbackScriptRuntime *runtime,
                                         const ForceFeedbackScriptInputs *inputs,
                                         ForceFeedbackScriptMotionState *state,
                                         uint32_t motion_ticks, int32_t wheel_position,
                                         uint32_t half_travel, bool integrate_inputs);

#endif
