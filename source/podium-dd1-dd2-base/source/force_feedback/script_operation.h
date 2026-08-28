#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_OPERATION_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_OPERATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_operand.h"

bool force_feedback_script_operation_execute(ForceFeedbackScriptRuntime *runtime, uint8_t operation,
                                             const uint8_t *script, size_t length, size_t *cursor,
                                             bool commit);

#endif
