#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_COMPARE_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_COMPARE_H

#include <stdint.h>

typedef uint8_t ForceFeedbackScriptComparison;

enum {
    FORCE_FEEDBACK_SCRIPT_GREATER_THAN = 0x40,
    FORCE_FEEDBACK_SCRIPT_GREATER_OR_EQUAL = 0x41,
    FORCE_FEEDBACK_SCRIPT_LESS_THAN = 0x42,
    FORCE_FEEDBACK_SCRIPT_LESS_OR_EQUAL = 0x43,
    FORCE_FEEDBACK_SCRIPT_NEGATIVE = 0x44,
    FORCE_FEEDBACK_SCRIPT_POSITIVE = 0x45,
};

float force_feedback_script_compare(ForceFeedbackScriptComparison comparison, float first,
                                    float second);

#endif
