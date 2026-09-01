#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_LOGIC_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_LOGIC_H

#include <stdint.h>

/**
 * @brief Identifies a logical operation in a force-feedback script.
 *
 * Values are the bytecode operation identifiers interpreted by the script engine.
 */
typedef uint8_t ForceFeedbackScriptLogicOperation;

/**
 * @brief Force-feedback script logical-operation opcodes.
 *
 * The opcodes use ordered nonzero floating-point values as boolean operands and return canonical
 * floating-point truth values.
 */
enum {
    FORCE_FEEDBACK_SCRIPT_LOGICAL_AND = 0x30,  /**< Computes logical AND of both operands. */
    FORCE_FEEDBACK_SCRIPT_LOGICAL_OR = 0x31,   /**< Computes logical OR of both operands. */
    FORCE_FEEDBACK_SCRIPT_LOGICAL_NAND = 0x32, /**< Computes logical NAND of both operands. */
    FORCE_FEEDBACK_SCRIPT_LOGICAL_NOR = 0x33,  /**< Computes logical NOR of both operands. */
    FORCE_FEEDBACK_SCRIPT_LOGICAL_XOR = 0x34,  /**< Computes logical XOR of both operands. */
    FORCE_FEEDBACK_SCRIPT_LOGICAL_NOT = 0x35,  /**< Computes logical NOT of the first operand. */
    FORCE_FEEDBACK_SCRIPT_LOGICAL_XNOR = 0x36, /**< Computes logical XNOR of both operands. */
};

/**
 * @brief Evaluate one force-feedback script logical operation.
 *
 * A finite nonzero value or infinity is true; both signed zeros and NaN are false. Logical NOT
 * uses only first, all other operations use both operands, and the result is always float 1.0 or
 * 0.0. An unknown opcode produces 0.0.
 *
 * @param[in] operation Logical-operation opcode to evaluate.
 * @param[in] first First or only floating-point operand.
 * @param[in] second Second floating-point operand for binary operations.
 * @return 1.0 when the logical result is true; otherwise 0.0.
 */
float force_feedback_script_logic_evaluate(ForceFeedbackScriptLogicOperation operation, float first,
                                           float second);

#endif
