#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_INTEGER_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_INTEGER_H

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t ForceFeedbackScriptIntegerOperation;

enum {
    FORCE_FEEDBACK_SCRIPT_INTEGER_U32_TO_FLOAT = 0xd0,
    FORCE_FEEDBACK_SCRIPT_INTEGER_FLOAT_TO_U32 = 0xd1,
    FORCE_FEEDBACK_SCRIPT_INTEGER_SUBTRACT_I32_TO_FLOAT = 0xd2,
    FORCE_FEEDBACK_SCRIPT_INTEGER_ABSOLUTE_DIFFERENCE = 0xd3,
    FORCE_FEEDBACK_SCRIPT_INTEGER_MODULO_TO_FLOAT = 0xd4,
    FORCE_FEEDBACK_SCRIPT_INTEGER_MODULO = 0xd5,
    FORCE_FEEDBACK_SCRIPT_INTEGER_DEGREES_TO_RADIANS = 0xd6,
};

typedef struct {
    uint32_t value;
    bool writes_value;
} ForceFeedbackScriptIntegerResult;

ForceFeedbackScriptIntegerResult
force_feedback_script_integer_evaluate(ForceFeedbackScriptIntegerOperation operation,
                                       uint32_t first, uint32_t second);

#endif
