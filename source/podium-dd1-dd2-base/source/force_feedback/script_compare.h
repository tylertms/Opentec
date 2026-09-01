#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_COMPARE_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_COMPARE_H

#include <stdint.h>

/**
 * @brief Identifies a floating-point comparison in a force-feedback script.
 *
 * Values are the bytecode operation identifiers interpreted by the script engine.
 */
typedef uint8_t ForceFeedbackScriptComparison;

/**
 * @brief Force-feedback script comparison opcodes.
 *
 * The first four opcodes compare two operands; the final two test the sign of the first operand.
 */
enum {
    FORCE_FEEDBACK_SCRIPT_GREATER_THAN = 0x40,     /**< Tests first > second. */
    FORCE_FEEDBACK_SCRIPT_GREATER_OR_EQUAL = 0x41, /**< Tests first >= second. */
    FORCE_FEEDBACK_SCRIPT_LESS_THAN = 0x42,        /**< Tests first < second. */
    FORCE_FEEDBACK_SCRIPT_LESS_OR_EQUAL = 0x43,    /**< Tests first <= second. */
    FORCE_FEEDBACK_SCRIPT_NEGATIVE = 0x44,         /**< Tests whether first is strictly negative. */
    FORCE_FEEDBACK_SCRIPT_POSITIVE = 0x45,         /**< Tests whether first is strictly positive. */
};

/**
 * @brief Evaluate one force-feedback script floating-point comparison.
 *
 * The ordered comparisons use both operands, while negative and positive use only first. The
 * result is always canonical float 1.0 or 0.0; unordered comparisons with NaN and unknown
 * opcodes produce 0.0.
 *
 * @param[in] comparison Comparison opcode to evaluate.
 * @param[in] first First or only floating-point operand.
 * @param[in] second Second floating-point operand for ordered comparisons.
 * @return 1.0 when the selected predicate is true; otherwise 0.0.
 */
float force_feedback_script_compare(ForceFeedbackScriptComparison comparison, float first,
                                    float second);

#endif
