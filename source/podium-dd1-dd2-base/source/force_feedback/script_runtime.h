#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_RUNTIME_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_RUNTIME_H

#include "force_feedback/script_operand.h"
#include "force_feedback/script_store.h"

typedef struct {
    ForceFeedbackScriptRuntime values;
    ForceFeedbackScriptStore store;
    ForceFeedbackScriptInputs inputs;
    ForceFeedbackScriptClock clock;
    ForceFeedbackRuntimeMode mode;
} ForceFeedbackScriptSystem;

void force_feedback_script_runtime_init(ForceFeedbackScriptSystem *system);

#endif
