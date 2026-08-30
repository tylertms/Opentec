#include "force_feedback/script_logic.h"

#include <stdbool.h>

/**
 * @brief Tests a script value for ordered nonzero truth.
 *
 * Positive and negative finite values are true. Both zeros and NaN are false.
 *
 * @param[in] value Floating-point script value.
 * @return true when the value is ordered and nonzero; otherwise false.
 */
static bool ordered_nonzero(float value) { return value < 0.0f || value > 0.0f; }

/**
 * @brief Converts a C truth value to the script truth representation.
 *
 * Produces canonical floating-point one or zero.
 *
 * @param[in] value Truth value to convert.
 * @return Floating-point one for true or zero for false.
 */
static float logical_value(bool value) { return value ? 1.0f : 0.0f; }

/**
 * @brief Evaluate a script logical operation.
 *
 * Implements VM opcodes 0x30 through 0x36. An ordered nonzero operand is true; positive zero,
 * negative zero, and NaN are false. Every operation produces the canonical float value 0 or 1.
 * Logical NOT consumes only the first operand, while all other operations consume both operands.
 *
 * @param[in] operation Logical opcode to evaluate.
 * @param[in] first First or only operand.
 * @param[in] second Second operand for binary operations.
 * @return Float 1 when the logical result is true, otherwise float 0.
 * @pre operation is a logical opcode from 0x30 through 0x36.
 */
float force_feedback_script_logic_evaluate(ForceFeedbackScriptLogicOperation operation, float first,
                                           float second) {
    bool first_value = ordered_nonzero(first);
    bool second_value = ordered_nonzero(second);

    switch (operation) {
    case FORCE_FEEDBACK_SCRIPT_LOGICAL_AND:
        return logical_value(first_value && second_value);
    case FORCE_FEEDBACK_SCRIPT_LOGICAL_OR:
        return logical_value(first_value || second_value);
    case FORCE_FEEDBACK_SCRIPT_LOGICAL_NAND:
        return logical_value(!(first_value && second_value));
    case FORCE_FEEDBACK_SCRIPT_LOGICAL_NOR:
        return logical_value(!(first_value || second_value));
    case FORCE_FEEDBACK_SCRIPT_LOGICAL_XOR:
        return logical_value(first_value != second_value);
    case FORCE_FEEDBACK_SCRIPT_LOGICAL_NOT:
        return logical_value(!first_value);
    case FORCE_FEEDBACK_SCRIPT_LOGICAL_XNOR:
        return logical_value(first_value == second_value);
    default:
        return 0.0f;
    }
}
