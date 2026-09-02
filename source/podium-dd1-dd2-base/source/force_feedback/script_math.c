#include "force_feedback/script_math.h"

#include <math.h>

/** @brief Single-precision pi used by the script angle operations. */
static const float script_pi = 3.1415927f;

/** @brief Largest finite tangent result accepted for a writable script result. */
static const float tangent_limit = 22875900.0f;

/**
 * @brief Creates a writable arithmetic result.
 *
 * Marks the supplied value for delivery to the encoded destination.
 *
 * @param[in] value Arithmetic result.
 * @return A writable arithmetic result containing the value.
 */
static ForceFeedbackScriptMathResult value_result(float value) {
    return (ForceFeedbackScriptMathResult){.value = value, .writes_value = true};
}

/**
 * @brief Creates a suppressed arithmetic result.
 *
 * Leaves the destination-write flag clear for rejected operations and undefined domains.
 *
 * @return An arithmetic result that suppresses its destination write.
 */
static ForceFeedbackScriptMathResult skipped_result(void) {
    return (ForceFeedbackScriptMathResult){0};
}

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
        return value_result(first > 0.0f ? 1.0f : first == 0.0f ? 0.0f : -1.0f);
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
        return value > tangent_limit || value < -tangent_limit ? skipped_result()
                                                               : value_result(value);
    }
    case FORCE_FEEDBACK_SCRIPT_MATH_MULTIPLY_PI:
        return value_result(first * script_pi);
    case FORCE_FEEDBACK_SCRIPT_MATH_DIVIDE_PI:
        return value_result(first / script_pi);
    case FORCE_FEEDBACK_SCRIPT_MATH_DEGREES_TO_RADIANS:
        return value_result(first * script_pi / 180.0f);
    case FORCE_FEEDBACK_SCRIPT_MATH_RADIANS_TO_DEGREES:
        return value_result(first * 180.0f / script_pi);
    case FORCE_FEEDBACK_SCRIPT_MATH_VECTOR_MAGNITUDE:
        return value_result(sqrtf(first * first + second * second));
    case FORCE_FEEDBACK_SCRIPT_MATH_MULTIPLY_COSINE:
        return value_result(first * cosf(second));
    case FORCE_FEEDBACK_SCRIPT_MATH_MULTIPLY_SINE:
        return value_result(first * sinf(second));
    default:
        return skipped_result();
    }
}
