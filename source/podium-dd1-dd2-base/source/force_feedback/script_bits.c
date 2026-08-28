#include "force_feedback/script_bits.h"

#include <stdint.h>

static ForceFeedbackScriptBitResult value_result(uint32_t value) {
    return (ForceFeedbackScriptBitResult){.value = value, .writes_value = true};
}

static ForceFeedbackScriptBitResult skipped_result(void) {
    return (ForceFeedbackScriptBitResult){0};
}

/**
 * @brief Evaluate a script bit operation on raw 32-bit values.
 *
 * Implements VM opcodes 0x50 through 0x59 without numeric conversion. Binary bitwise operations
 * consume both raw operands, while bitwise NOT consumes only the first. Test, set, and clear use
 * the second operand as an unsigned bit index from 0 through 31 and skip the destination write for
 * a larger index. Test-bit writes the raw float encoding of 0 or 1; all other operations write raw
 * integer results.
 *
 * @param[in] operation Bit opcode to evaluate.
 * @param[in] first First operand or value to inspect or modify.
 * @param[in] second Second operand or bit index.
 * @return The raw 32-bit result and whether the VM writes it to the destination.
 */
ForceFeedbackScriptBitResult
force_feedback_script_bits_evaluate(ForceFeedbackScriptBitOperation operation, uint32_t first,
                                    uint32_t second) {
    switch (operation) {
    case FORCE_FEEDBACK_SCRIPT_BITWISE_AND:
        return value_result(first & second);
    case FORCE_FEEDBACK_SCRIPT_BITWISE_OR:
        return value_result(first | second);
    case FORCE_FEEDBACK_SCRIPT_BITWISE_NAND:
        return value_result(~(first & second));
    case FORCE_FEEDBACK_SCRIPT_BITWISE_NOR:
        return value_result(~(first | second));
    case FORCE_FEEDBACK_SCRIPT_BITWISE_XOR:
        return value_result(first ^ second);
    case FORCE_FEEDBACK_SCRIPT_BITWISE_NOT:
        return value_result(~first);
    case FORCE_FEEDBACK_SCRIPT_BITWISE_XNOR:
        return value_result(~(first ^ second));
    case FORCE_FEEDBACK_SCRIPT_TEST_BIT:
        if (second < 32) {
            return value_result(first & (UINT32_C(1) << second) ? UINT32_C(0x3f800000) : 0);
        }
        return skipped_result();
    case FORCE_FEEDBACK_SCRIPT_SET_BIT:
        return second < 32 ? value_result(first | (UINT32_C(1) << second)) : skipped_result();
    case FORCE_FEEDBACK_SCRIPT_CLEAR_BIT:
        return second < 32 ? value_result(first & ~(UINT32_C(1) << second)) : skipped_result();
    default:
        return skipped_result();
    }
}
