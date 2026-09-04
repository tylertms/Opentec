#include "force_feedback/script_operation.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_bits.h"
#include "force_feedback/script_compare.h"
#include "force_feedback/script_integer.h"
#include "force_feedback/script_logic.h"
#include "force_feedback/script_math.h"
#include "force_feedback/script_range.h"
#include "force_feedback/script_sample.h"

/**
 * @brief Operation-specific operand and operation identifiers.
 *
 * The values identify direct sample bases and the operation records handled by this dispatcher.
 */
enum {
    OPERAND_SAMPLE_LOW = 0x14,           /**< Low-bank direct sample operand. */
    OPERAND_SAMPLE_HIGH = 0x15,          /**< High-bank direct sample operand. */
    OPERATION_COPY = 0xa0,               /**< Copies one source operand to a destination. */
    OPERATION_SAMPLE = 0xa1,             /**< Reads one sample at a base-relative index. */
    OPERATION_SAMPLE_WRAPPED = 0xa2,     /**< Reads one sample at a wrapped index. */
    OPERATION_SAMPLE_INTERPOLATE = 0xa3, /**< Interpolates a sample curve at a target value. */
    OPERATION_ROTATION_SCALE = 0xd7,     /**< Scales a value by the active rotation range. */
};

/**
 * @brief Provides numeric and raw-bit views of an operation value.
 *
 * Operation dispatch preserves script operand bits while converting floating-point operands and
 * results for delegated evaluators.
 */
typedef union {
    float number;  /**< Single-precision numeric view. */
    uint32_t bits; /**< Raw 32-bit representation. */
} OperationValue;

/**
 * @brief Creates a script-operation result.
 *
 * Records the offset following an operation together with its acceptance state.
 *
 * @param[in] cursor Offset following the operation.
 * @param[in] valid true when the operation was accepted; otherwise false.
 * @return An operation result containing the supplied cursor and state.
 */
static ForceFeedbackScriptDestinationResult operation_result(size_t cursor, bool valid) {
    return (ForceFeedbackScriptDestinationResult){.cursor = cursor, .valid = valid};
}

/**
 * @brief Writes an operation result through an encoded destination.
 *
 * Delegates destination decoding and optional state changes to the operand layer.
 *
 * @param[in,out] runtime Script state selected by the destination.
 * @param[in] script Complete encoded script.
 * @param[in] length Number of available script bytes.
 * @param[in] cursor Offset of the destination.
 * @param[in] value Raw result value.
 * @param[in] commit true to store the value; false to consume without writing.
 * @return The following cursor and consumption state; when commit is true, valid also requires a
 * supported destination.
 */
static ForceFeedbackScriptDestinationResult write_value(ForceFeedbackScriptRuntime *runtime,
                                                        const uint8_t *script, size_t length,
                                                        size_t cursor, uint32_t value,
                                                        bool commit) {
    return force_feedback_script_operand_write(runtime, script, length, cursor, value, commit);
}

/**
 * @brief Resolves the sample-table base encoded by an operand.
 *
 * Direct sample operands contribute their encoded index instead of the current sample value. Other
 * operands retain their normally resolved value.
 *
 * @param[in] runtime Script state referenced by the operand.
 * @param[in] script Complete encoded script.
 * @param[in] length Number of available script bytes.
 * @param[in] cursor Offset of the base operand.
 * @return The resolved base, following cursor, and validity state.
 */
static ForceFeedbackScriptOperandResult read_sample_base(const ForceFeedbackScriptRuntime *runtime,
                                                         const uint8_t *script, size_t length,
                                                         size_t cursor) {
    size_t offset = cursor;
    ForceFeedbackScriptOperandResult result =
        force_feedback_script_operand_read(runtime, script, length, cursor);
    if (!result.valid) {
        return result;
    }
    if (script[offset] == OPERAND_SAMPLE_LOW) {
        result.value = script[offset + 1];
    } else if (script[offset] == OPERAND_SAMPLE_HIGH) {
        result.value = (uint32_t)script[offset + 1] + 256u;
    }
    return result;
}

/**
 * @brief Identifies arithmetic operations that consume two source operands.
 *
 * Binary arithmetic covers add through modulo and vector magnitude through multiply-by-sine.
 *
 * @param[in] operation Encoded arithmetic operation.
 * @return true when two source operands are required; otherwise false.
 */
static bool math_is_binary(uint8_t operation) {
    return operation <= FORCE_FEEDBACK_SCRIPT_MATH_MODULO ||
           operation >= FORCE_FEEDBACK_SCRIPT_MATH_VECTOR_MAGNITUDE;
}

/**
 * @brief Executes one arithmetic operation.
 *
 * Decodes the required source operands, evaluates the arithmetic operation, and consumes or writes
 * its destination.
 *
 * @param[in,out] runtime Script state referenced by operands and the destination.
 * @param[in] operation Encoded arithmetic operation.
 * @param[in] script Complete encoded script.
 * @param[in] length Number of available script bytes.
 * @param[in] cursor Offset of the first source operand.
 * @param[in] commit true to write the result; false to consume without writing.
 * @return The following cursor and operation state; when commit is true, valid also requires a
 * supported destination.
 */
static ForceFeedbackScriptDestinationResult execute_math(ForceFeedbackScriptRuntime *runtime,
                                                         uint8_t operation, const uint8_t *script,
                                                         size_t length, size_t cursor,
                                                         bool commit) {
    ForceFeedbackScriptOperandResult first =
        force_feedback_script_operand_read(runtime, script, length, cursor);
    if (!first.valid) {
        return operation_result(first.cursor, false);
    }
    cursor = first.cursor;

    ForceFeedbackScriptOperandResult second = {.valid = true, .cursor = cursor};
    if (math_is_binary(operation)) {
        second = force_feedback_script_operand_read(runtime, script, length, cursor);
        cursor = second.cursor;
    }
    if (!second.valid) {
        return operation_result(cursor, false);
    }

    ForceFeedbackScriptMathResult result =
        force_feedback_script_math_evaluate(operation, (OperationValue){.bits = first.value}.number,
                                            (OperationValue){.bits = second.value}.number);
    return result.writes_value ? write_value(runtime, script, length, cursor,
                                             (OperationValue){.number = result.value}.bits, commit)
                               : operation_result(cursor, false);
}

/**
 * @brief Executes one floating-point logical operation.
 *
 * Logical NOT consumes one source; the other logical operations consume two. The result is written
 * as a floating-point truth value through the encoded destination.
 *
 * @param[in,out] runtime Script state referenced by operands and the destination.
 * @param[in] operation Encoded logical operation.
 * @param[in] script Complete encoded script.
 * @param[in] length Number of available script bytes.
 * @param[in] cursor Offset of the first source operand.
 * @param[in] commit true to write the result; false to consume without writing.
 * @return The following cursor and operation state; when commit is true, valid also requires a
 * supported destination.
 */
static ForceFeedbackScriptDestinationResult execute_logic(ForceFeedbackScriptRuntime *runtime,
                                                          uint8_t operation, const uint8_t *script,
                                                          size_t length, size_t cursor,
                                                          bool commit) {
    ForceFeedbackScriptOperandResult first =
        force_feedback_script_operand_read(runtime, script, length, cursor);
    if (!first.valid) {
        return operation_result(first.cursor, false);
    }
    cursor = first.cursor;

    ForceFeedbackScriptOperandResult second = {.valid = true, .cursor = cursor};
    if (operation != FORCE_FEEDBACK_SCRIPT_LOGICAL_NOT) {
        second = force_feedback_script_operand_read(runtime, script, length, cursor);
        cursor = second.cursor;
    }
    if (!second.valid) {
        return operation_result(cursor, false);
    }

    float result = force_feedback_script_logic_evaluate(
        operation, (OperationValue){.bits = first.value}.number,
        (OperationValue){.bits = second.value}.number);
    return write_value(runtime, script, length, cursor, (OperationValue){.number = result}.bits,
                       commit);
}

/**
 * @brief Executes one floating-point comparison.
 *
 * Ordered comparisons consume two sources. Sign tests consume one. The result is written as a
 * floating-point truth value through the encoded destination.
 *
 * @param[in,out] runtime Script state referenced by operands and the destination.
 * @param[in] operation Encoded comparison operation.
 * @param[in] script Complete encoded script.
 * @param[in] length Number of available script bytes.
 * @param[in] cursor Offset of the first source operand.
 * @param[in] commit true to write the result; false to consume without writing.
 * @return The following cursor and operation state; when commit is true, valid also requires a
 * supported destination.
 */
static ForceFeedbackScriptDestinationResult execute_comparison(ForceFeedbackScriptRuntime *runtime,
                                                               uint8_t operation,
                                                               const uint8_t *script, size_t length,
                                                               size_t cursor, bool commit) {
    ForceFeedbackScriptOperandResult first =
        force_feedback_script_operand_read(runtime, script, length, cursor);
    if (!first.valid) {
        return operation_result(first.cursor, false);
    }
    cursor = first.cursor;

    ForceFeedbackScriptOperandResult second = {.valid = true, .cursor = cursor};
    if (operation <= FORCE_FEEDBACK_SCRIPT_LESS_OR_EQUAL) {
        second = force_feedback_script_operand_read(runtime, script, length, cursor);
        cursor = second.cursor;
    }
    if (!second.valid) {
        return operation_result(cursor, false);
    }

    float result =
        force_feedback_script_compare(operation, (OperationValue){.bits = first.value}.number,
                                      (OperationValue){.bits = second.value}.number);
    return write_value(runtime, script, length, cursor, (OperationValue){.number = result}.bits,
                       commit);
}

/**
 * @brief Identifies bit operations that consume one source operand.
 *
 * Only bitwise NOT is unary.
 *
 * @param[in] operation Encoded bit operation.
 * @return true for bitwise NOT; otherwise false.
 */
static bool bit_is_unary(uint8_t operation) {
    return operation == FORCE_FEEDBACK_SCRIPT_BITWISE_NOT;
}

/**
 * @brief Identifies bit operations that update their source operand.
 *
 * Set-bit and clear-bit write their modified value back through the first source encoding.
 *
 * @param[in] operation Encoded bit operation.
 * @return true for set-bit or clear-bit; otherwise false.
 */
static bool bit_updates_source(uint8_t operation) {
    return operation == FORCE_FEEDBACK_SCRIPT_SET_BIT ||
           operation == FORCE_FEEDBACK_SCRIPT_CLEAR_BIT;
}

/**
 * @brief Executes one integer bit operation.
 *
 * Decodes one or two sources, evaluates the selected operation, and writes either through the
 * encoded destination or back through the first source for set-bit and clear-bit.
 *
 * @param[in,out] runtime Script state referenced by operands and destinations.
 * @param[in] operation Encoded bit operation.
 * @param[in] script Complete encoded script.
 * @param[in] length Number of available script bytes.
 * @param[in] cursor Offset of the first source operand.
 * @param[in] commit true to write the result; false to consume without writing.
 * @return The following cursor and operation state; when commit is true, valid also requires a
 * supported destination.
 */
static ForceFeedbackScriptDestinationResult execute_bits(ForceFeedbackScriptRuntime *runtime,
                                                         uint8_t operation, const uint8_t *script,
                                                         size_t length, size_t cursor,
                                                         bool commit) {
    size_t source_cursor = cursor;
    ForceFeedbackScriptOperandResult first =
        force_feedback_script_operand_read(runtime, script, length, cursor);
    if (!first.valid) {
        return operation_result(first.cursor, false);
    }
    cursor = first.cursor;

    ForceFeedbackScriptOperandResult second = {.valid = true, .cursor = cursor};
    if (!bit_is_unary(operation)) {
        second = force_feedback_script_operand_read(runtime, script, length, cursor);
        cursor = second.cursor;
    }
    if (!second.valid) {
        return operation_result(cursor, false);
    }

    ForceFeedbackScriptBitResult result =
        force_feedback_script_bits_evaluate(operation, first.value, second.value);
    if (!result.writes_value) {
        return operation_result(cursor, false);
    }
    if (bit_updates_source(operation)) {
        ForceFeedbackScriptDestinationResult write = force_feedback_script_operand_write(
            runtime, script, length, source_cursor, result.value, commit);
        write.cursor = cursor;
        return write;
    }
    return write_value(runtime, script, length, cursor, result.value, commit);
}

/**
 * @brief Identifies integer operations that consume two source operands.
 *
 * Subtract-as-float through unsigned modulo are binary; the conversion operations are unary.
 *
 * @param[in] operation Encoded integer operation.
 * @return true when two source operands are required; otherwise false.
 */
static bool integer_is_binary(uint8_t operation) {
    return operation >= FORCE_FEEDBACK_SCRIPT_INTEGER_SUBTRACT_I32_TO_FLOAT &&
           operation <= FORCE_FEEDBACK_SCRIPT_INTEGER_MODULO;
}

/**
 * @brief Executes one integer conversion or arithmetic operation.
 *
 * Decodes the required sources, evaluates the integer operation, and consumes or writes its
 * destination.
 *
 * @param[in,out] runtime Script state referenced by operands and the destination.
 * @param[in] operation Encoded integer operation.
 * @param[in] script Complete encoded script.
 * @param[in] length Number of available script bytes.
 * @param[in] cursor Offset of the first source operand.
 * @param[in] commit true to write the result; false to consume without writing.
 * @return The following cursor and operation state; when commit is true, valid also requires a
 * supported destination.
 */
static ForceFeedbackScriptDestinationResult execute_integer(ForceFeedbackScriptRuntime *runtime,
                                                            uint8_t operation,
                                                            const uint8_t *script, size_t length,
                                                            size_t cursor, bool commit) {
    ForceFeedbackScriptOperandResult first =
        force_feedback_script_operand_read(runtime, script, length, cursor);
    if (!first.valid) {
        return operation_result(first.cursor, false);
    }
    cursor = first.cursor;

    ForceFeedbackScriptOperandResult second = {.valid = true, .cursor = cursor};
    if (integer_is_binary(operation)) {
        second = force_feedback_script_operand_read(runtime, script, length, cursor);
        cursor = second.cursor;
    }
    if (!second.valid) {
        return operation_result(cursor, false);
    }

    ForceFeedbackScriptIntegerResult result =
        force_feedback_script_integer_evaluate(operation, first.value, second.value);
    return result.writes_value ? write_value(runtime, script, length, cursor, result.value, commit)
                               : operation_result(cursor, false);
}

/**
 * @brief Executes a copy or sample-table operation.
 *
 * Copy resolves one source. Sample reads resolve a base and index, with wrapped and interpolated
 * variants consuming a third operand before the destination.
 *
 * @param[in,out] runtime Script state referenced by operands and the destination.
 * @param[in] operation Encoded copy or sample operation.
 * @param[in] script Complete encoded script.
 * @param[in] length Number of available script bytes.
 * @param[in] cursor Offset of the first source operand.
 * @param[in] commit true to write the result; false to consume without writing.
 * @return The following cursor and operation state; when commit is true, valid also requires a
 * supported destination.
 */
static ForceFeedbackScriptDestinationResult execute_sample(ForceFeedbackScriptRuntime *runtime,
                                                           uint8_t operation, const uint8_t *script,
                                                           size_t length, size_t cursor,
                                                           bool commit) {
    ForceFeedbackScriptOperandResult base =
        operation == OPERATION_COPY
            ? force_feedback_script_operand_read(runtime, script, length, cursor)
            : read_sample_base(runtime, script, length, cursor);
    if (!base.valid) {
        return operation_result(base.cursor, false);
    }
    cursor = base.cursor;

    ForceFeedbackScriptSampleResult result;
    if (operation == OPERATION_COPY) {
        result = (ForceFeedbackScriptSampleResult){.value = base.value, .writes_value = true};
    } else {
        ForceFeedbackScriptOperandResult second =
            force_feedback_script_operand_read(runtime, script, length, cursor);
        if (!second.valid) {
            return operation_result(second.cursor, false);
        }
        cursor = second.cursor;
        if (operation == OPERATION_SAMPLE) {
            result = force_feedback_script_sample_read(&runtime->samples, base.value, second.value);
        } else {
            ForceFeedbackScriptOperandResult third =
                force_feedback_script_operand_read(runtime, script, length, cursor);
            if (!third.valid) {
                return operation_result(third.cursor, false);
            }
            cursor = third.cursor;
            if (operation == OPERATION_SAMPLE_WRAPPED) {
                result = force_feedback_script_sample_read_wrapped(&runtime->samples, base.value,
                                                                   second.value, third.value);
            } else {
                result = force_feedback_script_sample_interpolate(
                    &runtime->samples, base.value, second.value,
                    (OperationValue){.bits = third.value}.number);
            }
        }
    }
    return result.writes_value ? write_value(runtime, script, length, cursor, result.value, commit)
                               : operation_result(cursor, false);
}

/**
 * @brief Executes one range operation.
 *
 * Resolves lower bound, upper bound, and value operands before evaluating the range and consuming
 * or writing its destination.
 *
 * @param[in,out] runtime Script state referenced by operands and the destination.
 * @param[in] operation Encoded range operation.
 * @param[in] script Complete encoded script.
 * @param[in] length Number of available script bytes.
 * @param[in] cursor Offset of the lower-bound operand.
 * @param[in] commit true to write the result; false to consume without writing.
 * @return The following cursor and operation state; when commit is true, valid also requires a
 * supported destination.
 */
static ForceFeedbackScriptDestinationResult execute_range(ForceFeedbackScriptRuntime *runtime,
                                                          uint8_t operation, const uint8_t *script,
                                                          size_t length, size_t cursor,
                                                          bool commit) {
    ForceFeedbackScriptOperandResult lower =
        force_feedback_script_operand_read(runtime, script, length, cursor);
    if (!lower.valid) {
        return operation_result(lower.cursor, false);
    }
    ForceFeedbackScriptOperandResult upper =
        force_feedback_script_operand_read(runtime, script, length, lower.cursor);
    if (!upper.valid) {
        return operation_result(upper.cursor, false);
    }
    ForceFeedbackScriptOperandResult value =
        force_feedback_script_operand_read(runtime, script, length, upper.cursor);
    if (!value.valid) {
        return operation_result(value.cursor, false);
    }
    float result = force_feedback_script_range_evaluate(
        operation, (OperationValue){.bits = lower.value}.number,
        (OperationValue){.bits = upper.value}.number, (OperationValue){.bits = value.value}.number);
    return write_value(runtime, script, length, value.cursor,
                       (OperationValue){.number = result}.bits, commit);
}

/**
 * @brief Executes rotation-range scaling.
 *
 * Resolves one floating-point source, scales it from the active rotation range, and consumes or
 * writes the encoded destination. The runtime keeps the raw sensitivity byte separate from the
 * decoded extended range because the VM operation consumes the byte exactly as received.
 *
 * @param[in,out] runtime Script state containing the rotation range and destination state.
 * @param[in] script Complete encoded script.
 * @param[in] length Number of available script bytes.
 * @param[in] cursor Offset of the source operand.
 * @param[in] commit true to write the result; false to consume without writing.
 * @return The following cursor and operation state; when commit is true, valid also requires a
 * supported destination.
 */
static ForceFeedbackScriptDestinationResult
execute_rotation_scale(ForceFeedbackScriptRuntime *runtime, const uint8_t *script, size_t length,
                       size_t cursor, bool commit) {
    ForceFeedbackScriptOperandResult value =
        force_feedback_script_operand_read(runtime, script, length, cursor);
    if (!value.valid) {
        return operation_result(value.cursor, false);
    }
    uint8_t raw_sensitivity_code = runtime->rotation_range_code;
    float result = force_feedback_script_rotation_scale(
        (OperationValue){.bits = value.value}.number, raw_sensitivity_code,
        runtime->extended_rotation_range);
    return write_value(runtime, script, length, value.cursor,
                       (OperationValue){.number = result}.bits, commit);
}

/**
 * @brief Reports whether a script operation has implemented execution behavior.
 *
 * @param[in] operation Script operation code.
 * @return True when the operation is supported; otherwise false.
 */
bool force_feedback_script_operation_supported(uint8_t operation) {
    return (operation >= FORCE_FEEDBACK_SCRIPT_MATH_ADD &&
            operation <= FORCE_FEEDBACK_SCRIPT_MATH_RECIPROCAL) ||
           (operation >= FORCE_FEEDBACK_SCRIPT_MATH_SINE &&
            operation <= FORCE_FEEDBACK_SCRIPT_MATH_TANGENT) ||
           (operation >= FORCE_FEEDBACK_SCRIPT_MATH_MULTIPLY_PI &&
            operation <= FORCE_FEEDBACK_SCRIPT_MATH_RADIANS_TO_DEGREES) ||
           (operation >= FORCE_FEEDBACK_SCRIPT_MATH_VECTOR_MAGNITUDE &&
            operation <= FORCE_FEEDBACK_SCRIPT_MATH_MULTIPLY_SINE) ||
           (operation >= FORCE_FEEDBACK_SCRIPT_LOGICAL_AND &&
            operation <= FORCE_FEEDBACK_SCRIPT_LOGICAL_XNOR) ||
           (operation >= FORCE_FEEDBACK_SCRIPT_GREATER_THAN &&
            operation <= FORCE_FEEDBACK_SCRIPT_POSITIVE) ||
           (operation >= FORCE_FEEDBACK_SCRIPT_BITWISE_AND &&
            operation <= FORCE_FEEDBACK_SCRIPT_CLEAR_BIT) ||
           (operation >= OPERATION_COPY && operation <= OPERATION_SAMPLE_INTERPOLATE) ||
           (operation >= FORCE_FEEDBACK_SCRIPT_RANGE_CLASSIFY &&
            operation <= FORCE_FEEDBACK_SCRIPT_RANGE_NORMALIZE) ||
           (operation >= FORCE_FEEDBACK_SCRIPT_INTEGER_U32_TO_FLOAT &&
            operation <= FORCE_FEEDBACK_SCRIPT_INTEGER_DEGREES_TO_RADIANS) ||
           operation == OPERATION_ROTATION_SCALE;
}

ForceFeedbackScriptDestinationResult
force_feedback_script_operation_execute(ForceFeedbackScriptRuntime *runtime, uint8_t operation,
                                        const uint8_t *script, size_t length, size_t cursor,
                                        bool commit) {
    if ((operation >= FORCE_FEEDBACK_SCRIPT_MATH_ADD &&
         operation <= FORCE_FEEDBACK_SCRIPT_MATH_RECIPROCAL) ||
        (operation >= FORCE_FEEDBACK_SCRIPT_MATH_SINE &&
         operation <= FORCE_FEEDBACK_SCRIPT_MATH_TANGENT) ||
        (operation >= FORCE_FEEDBACK_SCRIPT_MATH_MULTIPLY_PI &&
         operation <= FORCE_FEEDBACK_SCRIPT_MATH_RADIANS_TO_DEGREES) ||
        (operation >= FORCE_FEEDBACK_SCRIPT_MATH_VECTOR_MAGNITUDE &&
         operation <= FORCE_FEEDBACK_SCRIPT_MATH_MULTIPLY_SINE)) {
        return execute_math(runtime, operation, script, length, cursor, commit);
    }
    if (operation >= FORCE_FEEDBACK_SCRIPT_LOGICAL_AND &&
        operation <= FORCE_FEEDBACK_SCRIPT_LOGICAL_XNOR) {
        return execute_logic(runtime, operation, script, length, cursor, commit);
    }
    if (operation >= FORCE_FEEDBACK_SCRIPT_GREATER_THAN &&
        operation <= FORCE_FEEDBACK_SCRIPT_POSITIVE) {
        return execute_comparison(runtime, operation, script, length, cursor, commit);
    }
    if (operation >= FORCE_FEEDBACK_SCRIPT_BITWISE_AND &&
        operation <= FORCE_FEEDBACK_SCRIPT_CLEAR_BIT) {
        return execute_bits(runtime, operation, script, length, cursor, commit);
    }
    if (operation >= OPERATION_COPY && operation <= OPERATION_SAMPLE_INTERPOLATE) {
        return execute_sample(runtime, operation, script, length, cursor, commit);
    }
    if (operation >= FORCE_FEEDBACK_SCRIPT_RANGE_CLASSIFY &&
        operation <= FORCE_FEEDBACK_SCRIPT_RANGE_NORMALIZE) {
        return execute_range(runtime, operation, script, length, cursor, commit);
    }
    if (operation >= FORCE_FEEDBACK_SCRIPT_INTEGER_U32_TO_FLOAT &&
        operation <= FORCE_FEEDBACK_SCRIPT_INTEGER_DEGREES_TO_RADIANS) {
        return execute_integer(runtime, operation, script, length, cursor, commit);
    }
    if (operation == OPERATION_ROTATION_SCALE) {
        return execute_rotation_scale(runtime, script, length, cursor, commit);
    }
    return operation_result(cursor, false);
}
