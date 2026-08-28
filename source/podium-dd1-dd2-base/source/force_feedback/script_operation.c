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

enum {
    OPERAND_SAMPLE_LOW = 0x14,
    OPERAND_SAMPLE_HIGH = 0x15,
    OPERATION_COPY = 0xa0,
    OPERATION_SAMPLE = 0xa1,
    OPERATION_SAMPLE_WRAPPED = 0xa2,
    OPERATION_SAMPLE_INTERPOLATE = 0xa3,
    OPERATION_ROTATION_SCALE = 0xd7,
};

typedef union {
    float number;
    uint32_t bits;
} OperationValue;

static ForceFeedbackScriptDestinationResult operation_result(size_t cursor, bool valid) {
    return (ForceFeedbackScriptDestinationResult){.cursor = cursor, .valid = valid};
}

static ForceFeedbackScriptDestinationResult write_value(ForceFeedbackScriptRuntime *runtime,
                                                        const uint8_t *script, size_t length,
                                                        size_t cursor, uint32_t value,
                                                        bool commit) {
    return force_feedback_script_operand_write(runtime, script, length, cursor, value, commit);
}

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

static bool math_is_binary(uint8_t operation) {
    return operation <= FORCE_FEEDBACK_SCRIPT_MATH_MODULO ||
           operation >= FORCE_FEEDBACK_SCRIPT_MATH_VECTOR_MAGNITUDE;
}

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

static bool bit_is_unary(uint8_t operation) {
    return operation == FORCE_FEEDBACK_SCRIPT_BITWISE_NOT;
}

static bool bit_updates_source(uint8_t operation) {
    return operation == FORCE_FEEDBACK_SCRIPT_SET_BIT ||
           operation == FORCE_FEEDBACK_SCRIPT_CLEAR_BIT;
}

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

static bool integer_is_binary(uint8_t operation) {
    return operation >= FORCE_FEEDBACK_SCRIPT_INTEGER_SUBTRACT_I32_TO_FLOAT &&
           operation <= FORCE_FEEDBACK_SCRIPT_INTEGER_MODULO;
}

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

static ForceFeedbackScriptDestinationResult
execute_rotation_scale(ForceFeedbackScriptRuntime *runtime, const uint8_t *script, size_t length,
                       size_t cursor, bool commit) {
    ForceFeedbackScriptOperandResult value =
        force_feedback_script_operand_read(runtime, script, length, cursor);
    if (!value.valid) {
        return operation_result(value.cursor, false);
    }
    float result = force_feedback_script_rotation_scale(
        (OperationValue){.bits = value.value}.number, runtime->rotation_range_code,
        runtime->extended_rotation_range);
    return write_value(runtime, script, length, value.cursor,
                       (OperationValue){.number = result}.bits, commit);
}

/**
 * @brief Execute one force-feedback script operation record.
 *
 * Decodes the operation's source operands, evaluates it through the corresponding high-level
 * arithmetic, logic, comparison, bit, sample, range, or integer module, and consumes its encoded
 * destination. Set-bit and clear-bit update their first source operand in place. When commit is
 * false, all operands are evaluated and consumed without changing runtime state.
 *
 * @param[in,out] runtime Script values, samples, slots, motion outputs, and axes.
 * @param[in] operation Operation byte preceding the operand sequence.
 * @param[in] script Complete script byte sequence containing the operands.
 * @param[in] length Number of available script bytes.
 * @param[in] cursor Offset of the first operand.
 * @param[in] commit true to write the result; false to consume the record without writing.
 * @return The following cursor and whether the operation and every operand are valid.
 * @pre runtime and script point to valid objects.
 */
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
