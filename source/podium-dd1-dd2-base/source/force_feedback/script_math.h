#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_MATH_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_MATH_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Identifies a floating-point arithmetic operation in script bytecode.
 *
 * Values are the operation bytes interpreted by the force-feedback script engine.
 */
typedef uint8_t ForceFeedbackScriptMathOperation;

/**
 * @brief Floating-point arithmetic operation opcodes.
 *
 * The opcodes select arithmetic, trigonometric, angle-conversion, and vector operations.
 */
enum {
    FORCE_FEEDBACK_SCRIPT_MATH_ADD = 0x10,      /**< Adds both operands. */
    FORCE_FEEDBACK_SCRIPT_MATH_SUBTRACT = 0x11, /**< Subtracts the second operand from the first. */
    FORCE_FEEDBACK_SCRIPT_MATH_MULTIPLY = 0x12, /**< Multiplies both operands. */
    FORCE_FEEDBACK_SCRIPT_MATH_DIVIDE = 0x13,   /**< Divides the first operand by the second. */
    FORCE_FEEDBACK_SCRIPT_MATH_MODULO = 0x14, /**< Computes the floored modulo of both operands. */
    FORCE_FEEDBACK_SCRIPT_MATH_SQUARE = 0x15, /**< Squares the first operand. */
    FORCE_FEEDBACK_SCRIPT_MATH_CUBE = 0x16,   /**< Cubes the first operand. */
    FORCE_FEEDBACK_SCRIPT_MATH_SQUARE_ROOT =
        0x17, /**< Computes the square root of the first operand. */
    FORCE_FEEDBACK_SCRIPT_MATH_SIGN =
        0x18, /**< Returns 1 for positive, 0 for zero, and -1 otherwise. */
    FORCE_FEEDBACK_SCRIPT_MATH_ABSOLUTE =
        0x19, /**< Computes the absolute value of the first operand. */
    FORCE_FEEDBACK_SCRIPT_MATH_RECIPROCAL =
        0x1a,                                  /**< Computes the reciprocal of the first operand. */
    FORCE_FEEDBACK_SCRIPT_MATH_SINE = 0x20,    /**< Computes the sine of the first operand. */
    FORCE_FEEDBACK_SCRIPT_MATH_COSINE = 0x21,  /**< Computes the cosine of the first operand. */
    FORCE_FEEDBACK_SCRIPT_MATH_TANGENT = 0x22, /**< Computes the tangent of the first operand. */
    FORCE_FEEDBACK_SCRIPT_MATH_MULTIPLY_PI = 0x2a, /**< Multiplies the first operand by pi. */
    FORCE_FEEDBACK_SCRIPT_MATH_DIVIDE_PI = 0x2b,   /**< Divides the first operand by pi. */
    FORCE_FEEDBACK_SCRIPT_MATH_DEGREES_TO_RADIANS =
        0x2c, /**< Converts the first operand from degrees to radians. */
    FORCE_FEEDBACK_SCRIPT_MATH_RADIANS_TO_DEGREES =
        0x2d, /**< Converts the first operand from radians to degrees. */
    FORCE_FEEDBACK_SCRIPT_MATH_VECTOR_MAGNITUDE =
        0xb0, /**< Computes the magnitude of the two operands. */
    FORCE_FEEDBACK_SCRIPT_MATH_MULTIPLY_COSINE =
        0xb1, /**< Multiplies the first operand by the cosine of the second. */
    FORCE_FEEDBACK_SCRIPT_MATH_MULTIPLY_SINE =
        0xb2, /**< Multiplies the first operand by the sine of the second. */
};

/**
 * @brief Result of evaluating a floating-point script arithmetic operation.
 *
 * The value is returned as a floating-point number, and writes_value controls whether the caller
 * may write it to the encoded destination.
 */
typedef struct {
    float value;       /**< Computed floating-point result. */
    bool writes_value; /**< Whether the result may be written to the destination. */
} ForceFeedbackScriptMathResult;

/**
 * @brief Evaluate one floating-point script arithmetic operation.
 *
 * Implements arithmetic opcodes 0x10 through 0x1a, trigonometric opcodes 0x20 through 0x22,
 * angle opcodes 0x2a through 0x2d, and vector operations 0xb0 through 0xb2. Binary operations use
 * both operands and unary operations use first. Vector magnitude computes the Euclidean magnitude,
 * while multiply-cosine and multiply-sine treat first as an amplitude and second as an angle.
 * Modulo uses a floored quotient. Division or modulo by zero, square root of a negative value,
 * reciprocal of zero, and a finite tangent outside the accepted range suppress the destination
 * write. Tangent writes NaN results, and sign maps unordered values to -1.
 *
 * @param[in] operation Arithmetic opcode to evaluate.
 * @param[in] first First or only operand.
 * @param[in] second Second operand for binary operations.
 * @return The computed value and write permission; writes_value is false for rejected or
 * unsupported operations.
 */
ForceFeedbackScriptMathResult
force_feedback_script_math_evaluate(ForceFeedbackScriptMathOperation operation, float first,
                                    float second);

#endif
