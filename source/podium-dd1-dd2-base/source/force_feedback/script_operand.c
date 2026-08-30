#include "force_feedback/script_operand.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

enum {
    OPERAND_CONSTANT_FLOAT_NEGATIVE_ONE = 0x04,
    OPERAND_IMMEDIATE_FIRST = 0x10,
    OPERAND_IMMEDIATE_LAST = 0x13,
    OPERAND_SAMPLE_LOW = 0x14,
    OPERAND_SAMPLE_HIGH = 0x15,
    OPERAND_PERCENT = 0x18,
    OPERAND_NEGATIVE_PERCENT = 0x19,
    OPERAND_PER_MILLE = 0x1a,
    OPERAND_NEGATIVE_PER_MILLE = 0x1b,
    OPERAND_VARIABLE_FIRST = 0x20,
    OPERAND_VARIABLE_LAST = 0x2b,
    OPERAND_VARIABLE_SAMPLE_FIRST = 0x30,
    OPERAND_VARIABLE_SAMPLE_LAST = 0x37,
    OPERAND_SLOT_VALUE_FIRST = 0x40,
    OPERAND_SLOT_VALUE_LAST = 0x43,
    OPERAND_SLOT_SAMPLE_FIRST = 0x44,
    OPERAND_SLOT_SAMPLE_LAST = 0x47,
    OPERAND_SLOT_DELTA_RATE = 0x48,
    OPERAND_SLOT_AVERAGE_RATE = 0x49,
    OPERAND_SLOT_EXECUTION_COUNT = 0x4a,
    OPERAND_SLOT_TICK_SNAPSHOT = 0x4b,
    OPERAND_MOTION_FIRST = 0x50,
    OPERAND_MOTION_PRIMARY = 0x50,
    OPERAND_MOTION_SECONDARY = 0x52,
    OPERAND_MOTION_ACCUMULATE = 0x53,
    OPERAND_MOTION_LAST = 0x57,
    OPERAND_AXIS_FIRST = 0x60,
    OPERAND_AXIS_LAST = 0x69,
};

typedef union {
    float number;
    uint32_t bits;
} OperandValue;

/**
 * @brief Checks whether a complete encoded value remains in a script.
 *
 * Uses subtraction after checking the cursor to avoid an overflow-prone end-position calculation.
 *
 * @param[in] length Number of available script bytes.
 * @param[in] cursor Offset of the value.
 * @param[in] size Number of bytes required.
 * @return true when the complete value is available; otherwise false.
 */
static bool available(size_t length, size_t cursor, size_t size) {
    return cursor <= length && size <= length - cursor;
}

/**
 * @brief Creates a rejected operand result.
 *
 * Preserves the cursor reached while decoding and leaves the validity flag clear.
 *
 * @param[in] cursor Offset reached while decoding.
 * @return An invalid operand result at the supplied cursor.
 */
static ForceFeedbackScriptOperandResult invalid_operand(size_t cursor) {
    return (ForceFeedbackScriptOperandResult){.cursor = cursor};
}

/**
 * @brief Creates an accepted operand result.
 *
 * Combines the resolved raw value with the offset following the encoded operand.
 *
 * @param[in] value Resolved raw operand value.
 * @param[in] cursor Offset following the operand.
 * @return A valid operand result containing the supplied value and cursor.
 */
static ForceFeedbackScriptOperandResult operand_value(uint32_t value, size_t cursor) {
    return (ForceFeedbackScriptOperandResult){
        .value = value,
        .cursor = cursor,
        .valid = true,
    };
}

/**
 * @brief Creates a destination result.
 *
 * Records the offset following a destination together with its acceptance state.
 *
 * @param[in] cursor Offset following the destination.
 * @param[in] valid true when the destination was accepted; otherwise false.
 * @return A destination result containing the supplied cursor and state.
 */
static ForceFeedbackScriptDestinationResult destination_result(size_t cursor, bool valid) {
    return (ForceFeedbackScriptDestinationResult){.cursor = cursor, .valid = valid};
}

/**
 * @brief Decodes an unsigned big-endian integer.
 *
 * Consumes one through four encoded bytes from most significant to least significant.
 *
 * @param[in] data Encoded integer bytes.
 * @param[in] size Number of bytes to decode.
 * @return The decoded unsigned value.
 */
static uint32_t read_big_endian(const uint8_t *data, size_t size) {
    uint32_t value = 0;
    for (size_t index = 0; index < size; index++) {
        value = (value << CHAR_BIT) | data[index];
    }
    return value;
}

/**
 * @brief Decodes a scaled numeric literal.
 *
 * Converts an unsigned big-endian integer to floating point and divides it by the selected scale.
 *
 * @param[in] data Encoded integer bytes.
 * @param[in] size Number of bytes to decode.
 * @param[in] scale Divisor applied to the decoded value.
 * @return The raw bit representation of the scaled floating-point value.
 */
static uint32_t scaled_immediate(const uint8_t *data, size_t size, float scale) {
    return (OperandValue){.number = (float)read_big_endian(data, size) / scale}.bits;
}

/**
 * @brief Selects an active-slot metric.
 *
 * Maps operand codes 0x48 through 0x4b to delta rate, average rate, execution count, or tick
 * snapshot respectively.
 *
 * @param[in] slot Active script slot.
 * @param[in] opcode Encoded slot-metric operand.
 * @return The selected raw metric value.
 */
static uint32_t slot_metric(const ForceFeedbackScriptSlot *slot, uint8_t opcode) {
    switch (opcode) {
    case OPERAND_SLOT_DELTA_RATE:
        return slot->delta_rate;
    case OPERAND_SLOT_AVERAGE_RATE:
        return slot->average_rate;
    case OPERAND_SLOT_EXECUTION_COUNT:
        return slot->execution_count;
    default:
        return slot->tick_snapshot;
    }
}

/**
 * @brief Read one encoded script operand.
 *
 * Resolves constants, big-endian immediates, scaled numeric literals, samples, variables, active
 * slot values and metrics, motion values, and axes. The returned cursor follows the complete
 * operand. Invalid or incomplete operands return an invalid result; incomplete input moves the
 * returned cursor to the end.
 *
 * @param[in] runtime Script state referenced by indirect operands.
 * @param[in] script Encoded script bytes.
 * @param[in] length Number of available script bytes.
 * @param[in] cursor Offset of the operand.
 * @return The resolved raw value, following cursor, and validity state.
 * @pre runtime and script point to valid objects.
 */
ForceFeedbackScriptOperandResult
force_feedback_script_operand_read(const ForceFeedbackScriptRuntime *runtime, const uint8_t *script,
                                   size_t length, size_t cursor) {
    static const uint32_t constants[] = {
        0, 1, UINT32_C(0x3f800000), UINT32_MAX, UINT32_C(0xbf800000),
    };
    if (!available(length, cursor, 1)) {
        return invalid_operand(length);
    }

    uint8_t opcode = script[cursor++];
    if (opcode <= OPERAND_CONSTANT_FLOAT_NEGATIVE_ONE) {
        return operand_value(constants[opcode], cursor);
    }
    if (opcode >= OPERAND_IMMEDIATE_FIRST && opcode <= OPERAND_IMMEDIATE_LAST) {
        size_t size = (size_t)(opcode - OPERAND_IMMEDIATE_FIRST) + 1;
        if (!available(length, cursor, size)) {
            return invalid_operand(length);
        }
        return operand_value(read_big_endian(script + cursor, size), cursor + size);
    }
    if (opcode == OPERAND_SAMPLE_LOW || opcode == OPERAND_SAMPLE_HIGH) {
        if (!available(length, cursor, 1)) {
            return invalid_operand(length);
        }
        size_t index = script[cursor] + (opcode == OPERAND_SAMPLE_HIGH ? 256u : 0u);
        return operand_value(runtime->samples.values[index], cursor + 1);
    }
    if (opcode >= OPERAND_PERCENT && opcode <= OPERAND_NEGATIVE_PERCENT) {
        if (!available(length, cursor, 1)) {
            return invalid_operand(length);
        }
        float scale = opcode == OPERAND_PERCENT ? 100.0f : -100.0f;
        return operand_value(scaled_immediate(script + cursor, 1, scale), cursor + 1);
    }
    if (opcode >= OPERAND_PER_MILLE && opcode <= OPERAND_NEGATIVE_PER_MILLE) {
        if (!available(length, cursor, 2)) {
            return invalid_operand(length);
        }
        float scale = opcode == OPERAND_PER_MILLE ? 1000.0f : -1000.0f;
        return operand_value(scaled_immediate(script + cursor, 2, scale), cursor + 2);
    }
    if (opcode >= OPERAND_VARIABLE_FIRST && opcode <= OPERAND_VARIABLE_LAST) {
        return operand_value(runtime->variables[opcode - OPERAND_VARIABLE_FIRST], cursor);
    }
    if (opcode >= OPERAND_VARIABLE_SAMPLE_FIRST && opcode <= OPERAND_VARIABLE_SAMPLE_LAST) {
        size_t variable = opcode - OPERAND_VARIABLE_SAMPLE_FIRST;
        return operand_value(runtime->samples.values[(uint8_t)runtime->variables[variable]],
                             cursor);
    }
    if (opcode >= OPERAND_SLOT_VALUE_FIRST && opcode <= OPERAND_SLOT_TICK_SNAPSHOT) {
        if (runtime->active_slot >= FORCE_FEEDBACK_SCRIPT_SLOT_COUNT) {
            return invalid_operand(cursor);
        }
        const ForceFeedbackScriptSlot *slot = &runtime->slots[runtime->active_slot];
        if (opcode <= OPERAND_SLOT_VALUE_LAST) {
            return operand_value(slot->values[opcode - OPERAND_SLOT_VALUE_FIRST], cursor);
        }
        if (opcode <= OPERAND_SLOT_SAMPLE_LAST) {
            size_t bank = opcode - OPERAND_SLOT_SAMPLE_FIRST;
            return operand_value(runtime->samples.values[(uint8_t)slot->values[bank]], cursor);
        }
        return operand_value(slot_metric(slot, opcode), cursor);
    }
    if (opcode >= OPERAND_MOTION_FIRST && opcode <= OPERAND_MOTION_LAST) {
        return operand_value(runtime->motion[opcode - OPERAND_MOTION_FIRST], cursor);
    }
    if (opcode >= OPERAND_AXIS_FIRST && opcode <= OPERAND_AXIS_LAST) {
        return operand_value(runtime->axes[opcode - OPERAND_AXIS_FIRST], cursor);
    }
    return invalid_operand(cursor);
}

/**
 * @brief Limits a finite force value to the normalized output range.
 *
 * Values above one become one and values below negative one become negative one.
 *
 * @param[in] value Force value to limit.
 * @return The value limited to the inclusive range from -1 to 1.
 */
static float clamp_force(float value) {
    if (value > 1.0f) {
        return 1.0f;
    }
    if (value < -1.0f) {
        return -1.0f;
    }
    return value;
}

/**
 * @brief Writes an operand through the active script slot.
 *
 * Codes 0x40 through 0x43 select one of four slot values. Codes 0x44 through 0x47 use the
 * corresponding slot value as a sample index.
 *
 * @param[in,out] runtime Script state containing the active slot and sample store.
 * @param[in] opcode Encoded active-slot destination.
 * @param[in] value Raw value to store.
 * @return true when an active slot accepted the destination; otherwise false.
 */
static bool write_slot_operand(ForceFeedbackScriptRuntime *runtime, uint8_t opcode,
                               uint32_t value) {
    if (runtime->active_slot >= FORCE_FEEDBACK_SCRIPT_SLOT_COUNT) {
        return false;
    }
    ForceFeedbackScriptSlot *slot = &runtime->slots[runtime->active_slot];
    if (opcode <= OPERAND_SLOT_VALUE_LAST) {
        slot->values[opcode - OPERAND_SLOT_VALUE_FIRST] = value;
        return true;
    }
    size_t bank = opcode - OPERAND_SLOT_SAMPLE_FIRST;
    runtime->samples.values[(uint8_t)slot->values[bank]] = value;
    return true;
}

/**
 * @brief Writes a motion destination.
 *
 * Stores primary or secondary motion directly. The accumulator adds a floating-point value to the
 * secondary motion output and limits finite results to the normalized force range.
 *
 * @param[in,out] runtime Script state containing motion outputs.
 * @param[in] opcode Encoded motion destination.
 * @param[in] value Raw value to store or accumulate.
 * @return true when the motion destination is writable; otherwise false.
 */
static bool write_motion_operand(ForceFeedbackScriptRuntime *runtime, uint8_t opcode,
                                 uint32_t value) {
    if (opcode == OPERAND_MOTION_PRIMARY) {
        runtime->motion[0] = value;
        return true;
    }
    if (opcode == OPERAND_MOTION_SECONDARY) {
        runtime->motion[2] = value;
        return true;
    }
    if (opcode == OPERAND_MOTION_ACCUMULATE) {
        float sum = (OperandValue){.bits = runtime->motion[2]}.number +
                    (OperandValue){.bits = value}.number;
        runtime->motion[2] = (OperandValue){.number = clamp_force(sum)}.bits;
        return true;
    }
    return false;
}

/**
 * @brief Write a value through one encoded script destination.
 *
 * Stores raw values in direct or indexed samples, writable variables, active-slot values, the
 * primary or secondary motion output, or axes. The accumulator destination adds floating-point
 * values to the secondary motion output and limits finite results to -1 through 1. When commit is
 * false, the destination is only consumed; direct sample operands still consume their index byte.
 *
 * @param[in,out] runtime Script state selected by the destination.
 * @param[in] script Encoded script bytes.
 * @param[in] length Number of available script bytes.
 * @param[in] cursor Offset of the destination.
 * @param[in] value Raw 32-bit value to store.
 * @param[in] commit true to change runtime state; false to consume without writing.
 * @return The following cursor and whether the destination was consumed and accepted.
 * @pre runtime and script point to valid objects.
 */
ForceFeedbackScriptDestinationResult
force_feedback_script_operand_write(ForceFeedbackScriptRuntime *runtime, const uint8_t *script,
                                    size_t length, size_t cursor, uint32_t value, bool commit) {
    if (!available(length, cursor, 1)) {
        return destination_result(length, false);
    }

    uint8_t opcode = script[cursor++];
    uint8_t sample = 0;
    if (opcode == OPERAND_SAMPLE_LOW || opcode == OPERAND_SAMPLE_HIGH) {
        if (!available(length, cursor, 1)) {
            return destination_result(length, false);
        }
        sample = script[cursor++];
    }
    if (!commit) {
        return destination_result(cursor, true);
    }

    bool valid = true;
    if (opcode == OPERAND_SAMPLE_LOW || opcode == OPERAND_SAMPLE_HIGH) {
        size_t index = sample + (opcode == OPERAND_SAMPLE_HIGH ? 256u : 0u);
        runtime->samples.values[index] = value;
    } else if (opcode >= OPERAND_VARIABLE_FIRST &&
               opcode < OPERAND_VARIABLE_FIRST + FORCE_FEEDBACK_SCRIPT_WRITABLE_VARIABLE_COUNT) {
        runtime->variables[opcode - OPERAND_VARIABLE_FIRST] = value;
    } else if (opcode >= OPERAND_VARIABLE_SAMPLE_FIRST && opcode <= OPERAND_VARIABLE_SAMPLE_LAST) {
        size_t variable = opcode - OPERAND_VARIABLE_SAMPLE_FIRST;
        runtime->samples.values[(uint8_t)runtime->variables[variable]] = value;
    } else if (opcode >= OPERAND_SLOT_VALUE_FIRST && opcode <= OPERAND_SLOT_SAMPLE_LAST) {
        valid = write_slot_operand(runtime, opcode, value);
    } else if (opcode >= OPERAND_MOTION_FIRST && opcode <= OPERAND_MOTION_LAST) {
        valid = write_motion_operand(runtime, opcode, value);
    } else if (opcode >= OPERAND_AXIS_FIRST && opcode <= OPERAND_AXIS_LAST) {
        runtime->axes[opcode - OPERAND_AXIS_FIRST] = value;
    } else {
        valid = false;
    }
    return destination_result(cursor, valid);
}
