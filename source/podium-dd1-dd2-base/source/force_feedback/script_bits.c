#include "force_feedback/script_bits.h"

#include <stdint.h>

/**
 * @brief Creates a writable bit-operation result.
 *
 * Marks the supplied raw value for delivery to the encoded destination.
 *
 * @param[in] value Raw operation result.
 * @return A writable bit-operation result containing the value.
 */
static ForceFeedbackScriptBitResult value_result(uint32_t value) {
    return (ForceFeedbackScriptBitResult){.value = value, .writes_value = true};
}

/**
 * @brief Creates a suppressed bit-operation result.
 *
 * Leaves the destination-write flag clear for rejected operations and bit indexes.
 *
 * @return A bit-operation result that suppresses its destination write.
 */
static ForceFeedbackScriptBitResult skipped_result(void) {
    return (ForceFeedbackScriptBitResult){0};
}

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
