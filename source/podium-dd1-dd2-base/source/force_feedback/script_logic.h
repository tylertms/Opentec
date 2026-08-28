#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_LOGIC_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_LOGIC_H

#include <stdint.h>

typedef uint8_t ForceFeedbackScriptLogicOperation;

enum {
    FORCE_FEEDBACK_SCRIPT_LOGICAL_AND = 0x30,
    FORCE_FEEDBACK_SCRIPT_LOGICAL_OR = 0x31,
    FORCE_FEEDBACK_SCRIPT_LOGICAL_NAND = 0x32,
    FORCE_FEEDBACK_SCRIPT_LOGICAL_NOR = 0x33,
    FORCE_FEEDBACK_SCRIPT_LOGICAL_XOR = 0x34,
    FORCE_FEEDBACK_SCRIPT_LOGICAL_NOT = 0x35,
    FORCE_FEEDBACK_SCRIPT_LOGICAL_XNOR = 0x36,
};

float force_feedback_script_logic_evaluate(ForceFeedbackScriptLogicOperation operation, float first,
                                           float second);

#endif
