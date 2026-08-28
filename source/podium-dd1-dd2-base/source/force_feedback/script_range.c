#include "force_feedback/script_range.h"

static const float script_pi = 3.1415927f;

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

/**
 * @brief Scale a script rotation value by the active control range.
 *
 * Interprets the raw range code as a signed byte. Codes 126 and 127 select the unsigned extended
 * range; every other code uses the sign-extended code itself. The selected range is multiplied by
 * 5, then the operation evaluates value * pi * range / 180 using pi encoded by 0x40490FDB.
 *
 * @param[in] value Script rotation value to scale.
 * @param[in] range_code Raw active control-range code.
 * @param[in] extended_range Extended control range selected by codes 126 and 127.
 * @return The scaled rotation in radians.
 */
float force_feedback_script_rotation_scale(float value, uint8_t range_code,
                                           uint16_t extended_range) {
    int32_t selected_range =
        (int8_t)range_code > 125 ? (int32_t)extended_range : (int8_t)range_code;
    float range_scale = (float)selected_range * 5.0f;
    return value * script_pi * range_scale / 180.0f;
}
