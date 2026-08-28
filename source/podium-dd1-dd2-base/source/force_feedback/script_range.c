#include "force_feedback/script_range.h"

/**
 * @brief Evaluate a script range operation.
 *
 * Classification returns -1 at or below the lower boundary, 0 strictly between the boundaries,
 * and 1 at or above the upper boundary. Bounded normalization uses the same inclusive boundary
 * results and evaluates (value - lower) / (upper - lower) only between them. Unbounded
 * normalization always evaluates that expression. Ordered comparisons preserve a NaN as an
 * interior bounded-normalization result, while classification maps an unclassified NaN to zero.
 *
 * @param[in] operation Range opcode from 0xa8 through 0xaa.
 * @param[in] lower Lower range boundary.
 * @param[in] upper Upper range boundary.
 * @param[in] value Value to classify or normalize.
 * @return The classification or normalized floating-point result.
 * @pre operation is a defined ForceFeedbackScriptRangeOperation value.
 */
float force_feedback_script_range_evaluate(ForceFeedbackScriptRangeOperation operation, float lower,
                                           float upper, float value) {
    if (operation == FORCE_FEEDBACK_SCRIPT_RANGE_NORMALIZE) {
        return (value - lower) / (upper - lower);
    }
    if (value <= lower) {
        return -1.0f;
    }
    if (value >= upper) {
        return 1.0f;
    }
    if (operation == FORCE_FEEDBACK_SCRIPT_RANGE_NORMALIZE_BOUNDED) {
        return (value - lower) / (upper - lower);
    }
    return 0.0f;
}
