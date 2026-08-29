#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_REPORT_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_REPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_operand.h"

enum {
    FORCE_FEEDBACK_SCRIPT_STATUS_RESPONSE_SIZE = 22,
    FORCE_FEEDBACK_SCRIPT_VALUES_RESPONSE_SIZE = 53,
};

bool force_feedback_script_status_report_encode(const ForceFeedbackScriptRuntime *runtime,
                                                ForceFeedbackRuntimeMode mode, uint8_t *sequence,
                                                uint8_t *response, size_t length);
bool force_feedback_script_values_report_encode(const ForceFeedbackScriptRuntime *runtime,
                                                uint8_t *sequence, uint8_t *response,
                                                size_t length);

#endif
