#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_INTEGER_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_INTEGER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Identifies an integer-oriented operation in a force-feedback script.
 *
 * Values are the bytecode operation identifiers interpreted by the script engine.
 */
typedef uint8_t ForceFeedbackScriptIntegerOperation;

/**
 * @brief Force-feedback script integer-operation opcodes.
 *
 * The opcodes convert between raw unsigned integers and floats, perform signed or unsigned
 * arithmetic, or convert unsigned degrees to radians.
 */
enum {
    FORCE_FEEDBACK_SCRIPT_INTEGER_U32_TO_FLOAT = 0xd0, /**< Converts first uint32_t to float. */
    FORCE_FEEDBACK_SCRIPT_INTEGER_FLOAT_TO_U32 = 0xd1, /**< Converts first float to uint32_t. */
    FORCE_FEEDBACK_SCRIPT_INTEGER_SUBTRACT_I32_TO_FLOAT = 0xd2, /**< Subtracts signed operands. */
    FORCE_FEEDBACK_SCRIPT_INTEGER_ABSOLUTE_DIFFERENCE = 0xd3,   /**< Returns unsigned difference. */
    FORCE_FEEDBACK_SCRIPT_INTEGER_MODULO_TO_FLOAT = 0xd4,       /**< Returns remainder as float. */
    FORCE_FEEDBACK_SCRIPT_INTEGER_MODULO = 0xd5,                /**< Returns unsigned remainder. */
    FORCE_FEEDBACK_SCRIPT_INTEGER_DEGREES_TO_RADIANS = 0xd6,    /**< Converts degrees to radians. */
};

/**
 * @brief Result of evaluating a force-feedback script integer operation.
 *
 * The value is kept in the raw 32-bit representation used by script operands. A false
 * writes_value suppresses the destination write, such as for a zero modulo divisor or an oversized
 * float-to-integer conversion.
 */
typedef struct {
    uint32_t value;    /**< Raw 32-bit operation result. */
    bool writes_value; /**< Whether the result may be written to the encoded destination. */
} ForceFeedbackScriptIntegerResult;

/**
 * @brief Evaluate one force-feedback script integer operation.
 *
 * Operands and results use raw 32-bit script representations. Integer-to-float conversions use
 * unsigned first, signed subtraction uses both operands as int32_t, and absolute difference and
 * modulo use unsigned operands. Float-to-uint32 truncates nonnegative values, maps negative values
 * to UINT32_MAX, maps NaN to zero, and suppresses values above the float represented by raw
 * bits 0x4dcccccd. Modulo suppresses its result when second is zero; degree conversion uses pi
 * equal to 3.1415927f. Unknown operations also suppress the destination write.
 *
 * @param[in] operation Integer-operation opcode to evaluate.
 * @param[in] first First or only raw operand.
 * @param[in] second Second raw operand for binary operations.
 * @return The raw result and whether it may be written to the destination.
 */
ForceFeedbackScriptIntegerResult
force_feedback_script_integer_evaluate(ForceFeedbackScriptIntegerOperation operation,
                                       uint32_t first, uint32_t second);

#endif
