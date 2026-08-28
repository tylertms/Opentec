#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_TICK_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_TICK_H

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/script_output.h"
#include "force_feedback/script_runtime.h"

typedef uint8_t ForceFeedbackScriptOutputPolicy;

enum {
    FORCE_FEEDBACK_SCRIPT_OUTPUT_NONE = 0,
    FORCE_FEEDBACK_SCRIPT_OUTPUT_ZERO = 1,
    FORCE_FEEDBACK_SCRIPT_OUTPUT_MOTION = 2,
};

typedef struct {
    bool wrote_output;
    bool outside_travel;
} ForceFeedbackScriptTickResult;

ForceFeedbackScriptOutputPolicy force_feedback_script_tick(ForceFeedbackScriptSystem *system,
                                                           uint32_t now, int32_t wheel_position,
                                                           uint32_t half_travel);
ForceFeedbackScriptTickResult force_feedback_script_tick_output(
    ForceFeedbackScriptSystem *system, ForceFeedbackScriptOutputState *output_state, uint32_t now,
    int32_t wheel_position, uint32_t half_travel, const ForceFeedbackScriptOutputConfig *config,
    ForceOutputReport *report);

#endif
