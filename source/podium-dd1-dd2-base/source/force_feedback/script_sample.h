#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_SAMPLE_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_SAMPLE_H

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/script_input.h"

typedef struct {
    uint32_t value;
    bool writes_value;
} ForceFeedbackScriptSampleResult;

ForceFeedbackScriptSampleResult
force_feedback_script_sample_read(const ForceFeedbackScriptSamples *samples, uint32_t base,
                                  uint32_t offset);
ForceFeedbackScriptSampleResult
force_feedback_script_sample_read_wrapped(const ForceFeedbackScriptSamples *samples, uint32_t base,
                                          uint32_t value, uint32_t period);

#endif
