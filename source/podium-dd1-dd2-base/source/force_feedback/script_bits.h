#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_BITS_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_BITS_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Identifies a raw 32-bit script bit operation.
 *
 * Values are the bytecode operation identifiers interpreted by the force-feedback script engine.
 */
typedef uint8_t ForceFeedbackScriptBitOperation;

/**
 * @brief Force-feedback script bit-operation opcodes.
 *
 * The opcodes select binary bitwise operations, unary bitwise NOT, or indexed bit access.
 */
enum {
    FORCE_FEEDBACK_SCRIPT_BITWISE_AND = 0x50,  /**< Computes the bitwise AND of both operands. */
    FORCE_FEEDBACK_SCRIPT_BITWISE_OR = 0x51,   /**< Computes the bitwise OR of both operands. */
    FORCE_FEEDBACK_SCRIPT_BITWISE_NAND = 0x52, /**< Computes the bitwise NAND of both operands. */
    FORCE_FEEDBACK_SCRIPT_BITWISE_NOR = 0x53,  /**< Computes the bitwise NOR of both operands. */
    FORCE_FEEDBACK_SCRIPT_BITWISE_XOR = 0x54,  /**< Computes the bitwise XOR of both operands. */
    FORCE_FEEDBACK_SCRIPT_BITWISE_NOT = 0x55, /**< Computes the bitwise NOT of the first operand. */
    FORCE_FEEDBACK_SCRIPT_BITWISE_XNOR = 0x56, /**< Computes the bitwise XNOR of both operands. */
    FORCE_FEEDBACK_SCRIPT_TEST_BIT = 0x57,     /**< Tests the bit selected by second in first. */
    FORCE_FEEDBACK_SCRIPT_SET_BIT = 0x58,      /**< Sets the bit selected by second in first. */
    FORCE_FEEDBACK_SCRIPT_CLEAR_BIT = 0x59,    /**< Clears the bit selected by second in first. */
};

/**
 * @brief Result of evaluating a force-feedback script bit operation.
 *
 * The value is kept in the raw 32-bit representation used by script operands. A false
 * writes_value suppresses the destination write while still representing a completed evaluation.
 */
typedef struct {
    uint32_t value;    /**< Raw 32-bit operation result or zero for a suppressed write. */
    bool writes_value; /**< Whether the result may be written to the encoded destination. */
} ForceFeedbackScriptBitResult;

/**
 * @brief Evaluate one force-feedback script bit operation.
 *
 * Binary operations use both raw operands, bitwise NOT uses only first, and test, set, and clear
 * use second as an unsigned bit index. Test-bit returns the raw float representation of zero or
 * one. An index outside bits 0 through 31 and an unknown opcode suppress the destination write.
 *
 * @param[in] operation Bit-operation opcode to evaluate.
 * @param[in] first First raw operand or value to inspect or modify.
 * @param[in] second Second raw operand or unsigned bit index.
 * @return The raw result and whether it may be written to the destination.
 */
ForceFeedbackScriptBitResult
force_feedback_script_bits_evaluate(ForceFeedbackScriptBitOperation operation, uint32_t first,
                                    uint32_t second);

#endif
