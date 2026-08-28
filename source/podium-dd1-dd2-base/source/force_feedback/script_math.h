#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_MATH_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_MATH_H

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t ForceFeedbackScriptMathOperation;

enum {
    FORCE_FEEDBACK_SCRIPT_MATH_ADD = 0x10,
    FORCE_FEEDBACK_SCRIPT_MATH_SUBTRACT = 0x11,
    FORCE_FEEDBACK_SCRIPT_MATH_MULTIPLY = 0x12,
    FORCE_FEEDBACK_SCRIPT_MATH_DIVIDE = 0x13,
    FORCE_FEEDBACK_SCRIPT_MATH_MODULO = 0x14,
    FORCE_FEEDBACK_SCRIPT_MATH_SQUARE = 0x15,
    FORCE_FEEDBACK_SCRIPT_MATH_CUBE = 0x16,
    FORCE_FEEDBACK_SCRIPT_MATH_SQUARE_ROOT = 0x17,
    FORCE_FEEDBACK_SCRIPT_MATH_SIGN = 0x18,
    FORCE_FEEDBACK_SCRIPT_MATH_ABSOLUTE = 0x19,
    FORCE_FEEDBACK_SCRIPT_MATH_RECIPROCAL = 0x1a,
};

typedef struct {
    float value;
    bool writes_value;
} ForceFeedbackScriptMathResult;

ForceFeedbackScriptMathResult
force_feedback_script_math_evaluate(ForceFeedbackScriptMathOperation operation, float first,
                                    float second);

#endif
