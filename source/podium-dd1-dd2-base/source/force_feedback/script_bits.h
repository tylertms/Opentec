#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_BITS_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_BITS_H

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t ForceFeedbackScriptBitOperation;

enum {
    FORCE_FEEDBACK_SCRIPT_BITWISE_AND = 0x50,
    FORCE_FEEDBACK_SCRIPT_BITWISE_OR = 0x51,
    FORCE_FEEDBACK_SCRIPT_BITWISE_NAND = 0x52,
    FORCE_FEEDBACK_SCRIPT_BITWISE_NOR = 0x53,
    FORCE_FEEDBACK_SCRIPT_BITWISE_XOR = 0x54,
    FORCE_FEEDBACK_SCRIPT_BITWISE_NOT = 0x55,
    FORCE_FEEDBACK_SCRIPT_BITWISE_XNOR = 0x56,
    FORCE_FEEDBACK_SCRIPT_TEST_BIT = 0x57,
    FORCE_FEEDBACK_SCRIPT_SET_BIT = 0x58,
    FORCE_FEEDBACK_SCRIPT_CLEAR_BIT = 0x59,
};

typedef struct {
    uint32_t value;
    bool writes_value;
} ForceFeedbackScriptBitResult;

ForceFeedbackScriptBitResult
force_feedback_script_bits_evaluate(ForceFeedbackScriptBitOperation operation, uint32_t first,
                                    uint32_t second);

#endif
