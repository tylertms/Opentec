#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_RANGE_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_RANGE_H

#include <stdint.h>

/**
 * @brief Identifies a range operation in script bytecode.
 *
 * Values are the operation bytes interpreted by the force-feedback script engine.
 */
typedef uint8_t ForceFeedbackScriptRangeOperation;

/**
 * @brief Script range-operation opcodes.
 *
 * The opcodes classify a value or normalize it within a bounded or unbounded range.
 */
enum {
    FORCE_FEEDBACK_SCRIPT_RANGE_CLASSIFY = 0xa8, /**< Classifies a value against two boundaries. */
    FORCE_FEEDBACK_SCRIPT_RANGE_NORMALIZE_BOUNDED =
        0xa9, /**< Normalizes a value with boundary limits. */
    FORCE_FEEDBACK_SCRIPT_RANGE_NORMALIZE =
        0xaa, /**< Normalizes a value without boundary limits. */
};

/**
 * @brief Evaluate one script range operation.
 *
 * Classification returns -1 at or below lower, 0 strictly between the boundaries, and 1 at or
 * above upper. Bounded normalization uses those boundary results and evaluates the normalized
 * expression only between them; unbounded normalization always evaluates that expression. A NaN
 * value therefore classifies as zero and remains NaN when normalized. An unknown operation returns
 * zero.
 *
 * @param[in] operation Range opcode to evaluate.
 * @param[in] lower Lower range boundary.
 * @param[in] upper Upper range boundary.
 * @param[in] value Value to classify or normalize.
 * @return The classification or normalized floating-point result.
 */
float force_feedback_script_range_evaluate(ForceFeedbackScriptRangeOperation operation, float lower,
                                           float upper, float value);

/**
 * @brief Scale a script value by the active rotation range.
 *
 * Interprets the encoded sensitivity as a signed byte, selects the decoded extended range only
 * for signed codes 126 and 127, multiplies that range by five degrees, and applies it to value as
 * a floating-point radian scale. Negative encoded bytes therefore remain negative manual ranges.
 *
 * @param[in] value Script rotation value to scale.
 * @param[in] raw_sensitivity_code Raw encoded signed sensitivity byte; signed 126 and 127 select
 * the extended range.
 * @param[in] extended_range Decoded rotation range for signed sensitivity codes 126 and 127.
 * @return The scaled rotation in radians.
 */
float force_feedback_script_rotation_scale(float value, uint8_t raw_sensitivity_code,
                                           uint16_t extended_range);

#endif
