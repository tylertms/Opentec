#include "force_feedback/script_math.h"

#include <math.h>

static ForceFeedbackScriptMathResult value_result(float value) {
    return (ForceFeedbackScriptMathResult){.value = value, .writes_value = true};
}

static ForceFeedbackScriptMathResult skipped_result(void) {
    return (ForceFeedbackScriptMathResult){0};
}

/**
 * @brief Evaluate a script arithmetic operation.
 *
 * Implements VM opcodes 0x10 through 0x1A. Binary operations consume both operands; unary
 * operations consume only the first. Division and modulo by zero, square root of a negative
 * value, and reciprocal of zero skip the destination write. Modulo uses a floored quotient.
 *
 * @param[in] operation Arithmetic opcode to evaluate.
 * @param[in] first First or only operand.
 * @param[in] second Second operand for binary operations.
 * @return The computed value and whether the VM writes it to the destination.
 */
ForceFeedbackScriptMathResult
force_feedback_script_math_evaluate(ForceFeedbackScriptMathOperation operation, float first,
                                    float second) {
    switch (operation) {
    case FORCE_FEEDBACK_SCRIPT_MATH_ADD:
        return value_result(first + second);
    case FORCE_FEEDBACK_SCRIPT_MATH_SUBTRACT:
        return value_result(first - second);
    case FORCE_FEEDBACK_SCRIPT_MATH_MULTIPLY:
        return value_result(first * second);
    case FORCE_FEEDBACK_SCRIPT_MATH_DIVIDE:
        return second == 0.0f ? skipped_result() : value_result(first / second);
    case FORCE_FEEDBACK_SCRIPT_MATH_MODULO:
        return second == 0.0f ? skipped_result()
                              : value_result(first - floorf(first / second) * second);
    case FORCE_FEEDBACK_SCRIPT_MATH_SQUARE:
        return value_result(first * first);
    case FORCE_FEEDBACK_SCRIPT_MATH_CUBE:
        return value_result(powf(first, 3.0f));
    case FORCE_FEEDBACK_SCRIPT_MATH_SQUARE_ROOT:
        return first < 0.0f ? skipped_result() : value_result(sqrtf(first));
    case FORCE_FEEDBACK_SCRIPT_MATH_SIGN:
        return value_result(first > 0.0f ? 1.0f : first < 0.0f ? -1.0f : 0.0f);
    case FORCE_FEEDBACK_SCRIPT_MATH_ABSOLUTE:
        return value_result(fabsf(first));
    case FORCE_FEEDBACK_SCRIPT_MATH_RECIPROCAL:
        return first == 0.0f ? skipped_result() : value_result(1.0f / first);
    default:
        return skipped_result();
    }
}
