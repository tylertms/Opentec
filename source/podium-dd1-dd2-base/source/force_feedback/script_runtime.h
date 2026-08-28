#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_RUNTIME_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_motion.h"
#include "force_feedback/script_operand.h"
#include "force_feedback/script_scheduler.h"
#include "force_feedback/script_store.h"

typedef struct {
    ForceFeedbackScriptRuntime values;
    ForceFeedbackScriptStore store;
    ForceFeedbackScriptInputs inputs;
    ForceFeedbackScriptClock clock;
    ForceFeedbackScriptMotionState motion;
    ForceFeedbackScriptScheduler scheduler;
    uint32_t host_tick_snapshot;
    uint32_t idle_tick_snapshot;
    volatile ForceFeedbackRuntimeMode mode;
} ForceFeedbackScriptSystem;

void force_feedback_script_runtime_init(ForceFeedbackScriptSystem *system);
bool force_feedback_script_runtime_apply_control(ForceFeedbackScriptSystem *system,
                                                 const uint8_t *packet, size_t length);

#endif
