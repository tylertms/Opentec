#include "force_feedback/script_operand.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

enum {
    OPERAND_CONSTANT_ZERO = 0x00,
    OPERAND_CONSTANT_ONE = 0x01,
    OPERAND_CONSTANT_FLOAT_ONE = 0x02,
    OPERAND_CONSTANT_MAXIMUM = 0x03,
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

static bool consume(size_t length, size_t *cursor, size_t size) {
    if (*cursor > length || size > length - *cursor) {
        *cursor = length;
        return false;
    }
    *cursor += size;
    return true;
}

static uint32_t read_big_endian(const uint8_t *data, size_t size) {
    uint32_t value = 0;
    for (size_t index = 0; index < size; index++) {
        value = (value << CHAR_BIT) | data[index];
    }
    return value;
}

static uint32_t scaled_immediate(const uint8_t *data, size_t size, float scale) {
    return (OperandValue){.number = (float)read_big_endian(data, size) / scale}.bits;
}

static bool read_constant(uint8_t opcode, uint32_t *value) {
    static const uint32_t constants[] = {
        0, 1, UINT32_C(0x3f800000), UINT32_MAX, UINT32_C(0xbf800000),
    };
    if (opcode > OPERAND_CONSTANT_FLOAT_NEGATIVE_ONE) {
        return false;
    }
    *value = constants[opcode];
    return true;
}

static bool read_slot_metric(const ForceFeedbackScriptSlot *slot, uint8_t opcode, uint32_t *value) {
    switch (opcode) {
    case OPERAND_SLOT_DELTA_RATE:
        *value = slot->delta_rate;
        return true;
    case OPERAND_SLOT_AVERAGE_RATE:
        *value = slot->average_rate;
        return true;
    case OPERAND_SLOT_EXECUTION_COUNT:
        *value = slot->execution_count;
        return true;
    case OPERAND_SLOT_TICK_SNAPSHOT:
        *value = slot->tick_snapshot;
        return true;
    default:
        return false;
    }
}

/**
 * @brief Read one encoded script operand.
 *
 * Resolves constants, big-endian immediates, scaled numeric literals, samples, variables, active
 * slot values and metrics, motion values, and axes. The cursor advances past the complete operand.
 * Invalid or incomplete operands return false; incomplete input moves the cursor to the end.
 *
 * @param[in] runtime Script state referenced by indirect operands.
 * @param[in] script Encoded script bytes.
 * @param[in] length Number of available script bytes.
 * @param[in,out] cursor Offset of the operand on entry and the next byte on success.
 * @param[out] value Resolved raw 32-bit value.
 * @return true when the operand is complete and valid; otherwise false.
 * @pre runtime, script, cursor, and value point to valid objects.
 */
bool force_feedback_script_operand_read(const ForceFeedbackScriptRuntime *runtime,
                                        const uint8_t *script, size_t length, size_t *cursor,
                                        uint32_t *value) {
    size_t offset = *cursor;
    if (!consume(length, cursor, 1)) {
        return false;
    }

    uint8_t opcode = script[offset];
    if (read_constant(opcode, value)) {
        return true;
    }
    if (opcode >= OPERAND_IMMEDIATE_FIRST && opcode <= OPERAND_IMMEDIATE_LAST) {
        size_t size = (size_t)(opcode - OPERAND_IMMEDIATE_FIRST) + 1;
        offset = *cursor;
        return consume(length, cursor, size)
                   ? (*value = read_big_endian(script + offset, size), true)
                   : false;
    }
    if (opcode == OPERAND_SAMPLE_LOW || opcode == OPERAND_SAMPLE_HIGH) {
        offset = *cursor;
        if (!consume(length, cursor, 1)) {
            return false;
        }
        size_t index = script[offset] + (opcode == OPERAND_SAMPLE_HIGH ? 256u : 0u);
        *value = runtime->samples.values[index];
        return true;
    }
    if (opcode >= OPERAND_PERCENT && opcode <= OPERAND_NEGATIVE_PERCENT) {
        offset = *cursor;
        if (!consume(length, cursor, 1)) {
            return false;
        }
        float scale = opcode == OPERAND_PERCENT ? 100.0f : -100.0f;
        *value = scaled_immediate(script + offset, 1, scale);
        return true;
    }
    if (opcode >= OPERAND_PER_MILLE && opcode <= OPERAND_NEGATIVE_PER_MILLE) {
        offset = *cursor;
        if (!consume(length, cursor, 2)) {
            return false;
        }
        float scale = opcode == OPERAND_PER_MILLE ? 1000.0f : -1000.0f;
        *value = scaled_immediate(script + offset, 2, scale);
        return true;
    }
    if (opcode >= OPERAND_VARIABLE_FIRST && opcode <= OPERAND_VARIABLE_LAST) {
        *value = runtime->variables[opcode - OPERAND_VARIABLE_FIRST];
        return true;
    }
    if (opcode >= OPERAND_VARIABLE_SAMPLE_FIRST && opcode <= OPERAND_VARIABLE_SAMPLE_LAST) {
        size_t variable = opcode - OPERAND_VARIABLE_SAMPLE_FIRST;
        *value = runtime->samples.values[(uint8_t)runtime->variables[variable]];
        return true;
    }

    if (opcode >= OPERAND_SLOT_VALUE_FIRST && opcode <= OPERAND_SLOT_TICK_SNAPSHOT) {
        if (runtime->active_slot >= FORCE_FEEDBACK_SCRIPT_SLOT_COUNT) {
            return false;
        }
        const ForceFeedbackScriptSlot *slot = &runtime->slots[runtime->active_slot];
        if (opcode <= OPERAND_SLOT_VALUE_LAST) {
            *value = slot->values[opcode - OPERAND_SLOT_VALUE_FIRST];
            return true;
        }
        if (opcode <= OPERAND_SLOT_SAMPLE_LAST) {
            size_t bank = opcode - OPERAND_SLOT_SAMPLE_FIRST;
            *value = runtime->samples.values[(uint8_t)slot->values[bank]];
            return true;
        }
        return read_slot_metric(slot, opcode, value);
    }
    if (opcode >= OPERAND_MOTION_FIRST && opcode <= OPERAND_MOTION_LAST) {
        *value = runtime->motion[opcode - OPERAND_MOTION_FIRST];
        return true;
    }
    if (opcode >= OPERAND_AXIS_FIRST && opcode <= OPERAND_AXIS_LAST) {
        *value = runtime->axes[opcode - OPERAND_AXIS_FIRST];
        return true;
    }
    return false;
}

static float clamp_force(float value) {
    if (value > 1.0f) {
        return 1.0f;
    }
    if (value < -1.0f) {
        return -1.0f;
    }
    return value;
}

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
 * @param[in,out] cursor Offset of the destination on entry and the next byte on return.
 * @param[in] value Raw 32-bit value to store.
 * @param[in] commit true to change runtime state; false to consume without writing.
 * @return true when the destination was consumed and accepted; otherwise false.
 * @pre runtime, script, and cursor point to valid objects.
 */
bool force_feedback_script_operand_write(ForceFeedbackScriptRuntime *runtime, const uint8_t *script,
                                         size_t length, size_t *cursor, uint32_t value,
                                         bool commit) {
    size_t offset = *cursor;
    if (!consume(length, cursor, 1)) {
        return false;
    }

    uint8_t opcode = script[offset];
    uint8_t sample = 0;
    if (opcode == OPERAND_SAMPLE_LOW || opcode == OPERAND_SAMPLE_HIGH) {
        offset = *cursor;
        if (!consume(length, cursor, 1)) {
            return false;
        }
        sample = script[offset];
    }
    if (!commit) {
        return true;
    }
    if (opcode == OPERAND_SAMPLE_LOW || opcode == OPERAND_SAMPLE_HIGH) {
        size_t index = sample + (opcode == OPERAND_SAMPLE_HIGH ? 256u : 0u);
        runtime->samples.values[index] = value;
        return true;
    }
    if (opcode >= OPERAND_VARIABLE_FIRST &&
        opcode < OPERAND_VARIABLE_FIRST + FORCE_FEEDBACK_SCRIPT_WRITABLE_VARIABLE_COUNT) {
        runtime->variables[opcode - OPERAND_VARIABLE_FIRST] = value;
        return true;
    }
    if (opcode >= OPERAND_VARIABLE_SAMPLE_FIRST && opcode <= OPERAND_VARIABLE_SAMPLE_LAST) {
        size_t variable = opcode - OPERAND_VARIABLE_SAMPLE_FIRST;
        runtime->samples.values[(uint8_t)runtime->variables[variable]] = value;
        return true;
    }
    if (opcode >= OPERAND_SLOT_VALUE_FIRST && opcode <= OPERAND_SLOT_SAMPLE_LAST) {
        return write_slot_operand(runtime, opcode, value);
    }
    if (opcode >= OPERAND_MOTION_FIRST && opcode <= OPERAND_MOTION_LAST) {
        return write_motion_operand(runtime, opcode, value);
    }
    if (opcode >= OPERAND_AXIS_FIRST && opcode <= OPERAND_AXIS_LAST) {
        runtime->axes[opcode - OPERAND_AXIS_FIRST] = value;
        return true;
    }
    return false;
}
