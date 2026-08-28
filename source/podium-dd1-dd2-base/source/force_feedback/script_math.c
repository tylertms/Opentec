#include "force_feedback/script_math.h"

#include <math.h>

static const float script_pi = 3.1415927f;
static const float tangent_limit = 22875900.0f;

static ForceFeedbackScriptMathResult value_result(float value) {
    return (ForceFeedbackScriptMathResult){.value = value, .writes_value = true};
}

static ForceFeedbackScriptMathResult skipped_result(void) {
    return (ForceFeedbackScriptMathResult){0};
}

/**
 * @brief Evaluate a script arithmetic operation.
 *
 * Implements VM arithmetic opcodes 0x10 through 0x1A, trigonometric opcodes 0x20 through 0x22,
 * and angle opcodes 0x2A through 0x2D. Binary operations consume both operands; unary operations
 * consume only the first. Division and modulo by zero, square root of a negative value, reciprocal
 * of zero, and a tangent result outside the inclusive range -22875900 through 22875900 skip the
 * destination write. Modulo uses a floored quotient. Angle conversion uses 180 and the float value
 * of pi encoded as 0x40490FDB.
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
    case FORCE_FEEDBACK_SCRIPT_MATH_SINE:
        return value_result(sinf(first));
    case FORCE_FEEDBACK_SCRIPT_MATH_COSINE:
        return value_result(cosf(first));
    case FORCE_FEEDBACK_SCRIPT_MATH_TANGENT: {
        float value = tanf(first);
        return value >= -tangent_limit && value <= tangent_limit ? value_result(value)
                                                                 : skipped_result();
    }
    case FORCE_FEEDBACK_SCRIPT_MATH_MULTIPLY_PI:
        return value_result(first * script_pi);
    case FORCE_FEEDBACK_SCRIPT_MATH_DIVIDE_PI:
        return value_result(first / script_pi);
    case FORCE_FEEDBACK_SCRIPT_MATH_DEGREES_TO_RADIANS:
        return value_result(first * script_pi / 180.0f);
    case FORCE_FEEDBACK_SCRIPT_MATH_RADIANS_TO_DEGREES:
        return value_result(first * 180.0f / script_pi);
    default:
        return skipped_result();
    }
}
