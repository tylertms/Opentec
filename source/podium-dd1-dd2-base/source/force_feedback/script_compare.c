#include "force_feedback/script_compare.h"

#include <stdbool.h>

static float comparison_value(bool value) { return value ? 1.0f : 0.0f; }

/**
 * @brief Evaluate a script floating-point comparison.
 *
 * Implements VM opcodes 0x40 through 0x45. The four binary operations compare both operands.
 * Negative and positive test whether the first operand is strictly below or above zero. Every
 * operation produces canonical float 0 or 1, and every comparison involving NaN produces 0.
 *
 * @param[in] comparison Comparison opcode to evaluate.
 * @param[in] first First or only operand.
 * @param[in] second Second operand for binary comparisons.
 * @return Float 1 when the predicate is true, otherwise float 0.
 * @pre comparison is an assigned comparison opcode from 0x40 through 0x45.
 */
float force_feedback_script_compare(ForceFeedbackScriptComparison comparison, float first,
                                    float second) {
    switch (comparison) {
    case FORCE_FEEDBACK_SCRIPT_GREATER_THAN:
        return comparison_value(first > second);
    case FORCE_FEEDBACK_SCRIPT_GREATER_OR_EQUAL:
        return comparison_value(first >= second);
    case FORCE_FEEDBACK_SCRIPT_LESS_THAN:
        return comparison_value(first < second);
    case FORCE_FEEDBACK_SCRIPT_LESS_OR_EQUAL:
        return comparison_value(first <= second);
    case FORCE_FEEDBACK_SCRIPT_NEGATIVE:
        return comparison_value(first < 0.0f);
    case FORCE_FEEDBACK_SCRIPT_POSITIVE:
        return comparison_value(first > 0.0f);
    default:
        return 0.0f;
    }
}
