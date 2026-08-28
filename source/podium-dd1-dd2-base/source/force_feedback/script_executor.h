#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_EXECUTOR_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_EXECUTOR_H

#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_operand.h"

typedef uint8_t ForceFeedbackScriptExecutionStatus;

enum {
    FORCE_FEEDBACK_SCRIPT_EXECUTION_FINISHED = 0,
    FORCE_FEEDBACK_SCRIPT_EXECUTION_STOPPED = 1,
    FORCE_FEEDBACK_SCRIPT_EXECUTION_COMPLETED = 2,
    FORCE_FEEDBACK_SCRIPT_EXECUTION_FAULT = UINT8_MAX,
};

ForceFeedbackScriptExecutionStatus
force_feedback_script_execute(ForceFeedbackScriptRuntime *runtime, const uint8_t *script,
                              size_t length);

#endif
