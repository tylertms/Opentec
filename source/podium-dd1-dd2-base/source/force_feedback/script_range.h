#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_RANGE_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_RANGE_H

#include <stdint.h>

typedef uint8_t ForceFeedbackScriptRangeOperation;

enum {
    FORCE_FEEDBACK_SCRIPT_RANGE_CLASSIFY = 0xa8,
    FORCE_FEEDBACK_SCRIPT_RANGE_NORMALIZE_BOUNDED = 0xa9,
    FORCE_FEEDBACK_SCRIPT_RANGE_NORMALIZE = 0xaa,
};

float force_feedback_script_range_evaluate(ForceFeedbackScriptRangeOperation operation, float lower,
                                           float upper, float value);
float force_feedback_script_rotation_scale(float value, uint8_t range_code,
                                           uint16_t extended_range);

#endif
