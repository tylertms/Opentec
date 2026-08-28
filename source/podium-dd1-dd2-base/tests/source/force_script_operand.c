#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "force_feedback/script_operand.h"

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint32_t read_operand(const ForceFeedbackScriptRuntime *runtime, const uint8_t *script,
                             size_t length, size_t *cursor) {
    ForceFeedbackScriptOperandResult result =
        force_feedback_script_operand_read(runtime, script, length, *cursor);
    assert(result.valid);
    *cursor = result.cursor;
    return result.value;
}

static void test_reads_constants_and_immediates(void) {
    ForceFeedbackScriptRuntime runtime = {0};
    const uint8_t script[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x10, 0x7e, 0x11, 0x12, 0x34,
        0x12, 0x56, 0x78, 0x9a, 0x13, 0xbc, 0xde, 0xf0, 0x12,
    };
    const uint32_t expected[] = {
        0,
        1,
        UINT32_C(0x3f800000),
        UINT32_MAX,
        UINT32_C(0xbf800000),
        UINT32_C(0x7e),
        UINT32_C(0x1234),
        UINT32_C(0x56789a),
        UINT32_C(0xbcdef012),
    };
    size_t cursor = 0;
    for (size_t index = 0; index < sizeof(expected) / sizeof(expected[0]); index++) {
        assert(read_operand(&runtime, script, sizeof(script), &cursor) == expected[index]);
    }
    assert(cursor == sizeof(script));
}

static void test_reads_samples_and_scaled_literals(void) {
    ForceFeedbackScriptRuntime runtime = {0};
    runtime.samples.values[42] = UINT32_C(0x11223344);
    runtime.samples.values[298] = UINT32_C(0x55667788);
    const uint8_t script[] = {
        0x14, 42, 0x15, 42, 0x18, 75, 0x19, 25, 0x1a, 0x03, 0xe8, 0x1b, 0x01, 0xf4,
    };
    const uint32_t expected[] = {
        UINT32_C(0x11223344), UINT32_C(0x55667788), float_bits(0.75f),
        float_bits(-0.25f),   float_bits(1.0f),     float_bits(-0.5f),
    };
    size_t cursor = 0;
    for (size_t index = 0; index < sizeof(expected) / sizeof(expected[0]); index++) {
        assert(read_operand(&runtime, script, sizeof(script), &cursor) == expected[index]);
    }
    assert(cursor == sizeof(script));
}

static ForceFeedbackScriptRuntime prepare_runtime(void) {
    ForceFeedbackScriptRuntime runtime = {0};
    runtime.active_slot = 3;
    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_VARIABLE_COUNT; index++) {
        runtime.variables[index] = UINT32_C(0x10000000) + index;
    }
    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_MOTION_VALUE_COUNT; index++) {
        runtime.motion[index] = UINT32_C(0x20000000) + index;
    }
    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_AXIS_VALUE_COUNT; index++) {
        runtime.axes[index] = UINT32_C(0x30000000) + index;
    }
    for (uint8_t bank = 0; bank < 4; bank++) {
        runtime.slots[3].values[bank] = UINT32_C(0x40000000) + bank;
    }
    runtime.slots[3].delta_rate = UINT32_C(0x50000000);
    runtime.slots[3].average_rate = UINT32_C(0x50000001);
    runtime.slots[3].execution_count = UINT32_C(0x50000002);
    runtime.slots[3].tick_snapshot = UINT32_C(0x50000003);
    return runtime;
}

static void test_reads_runtime_banks(void) {
    ForceFeedbackScriptRuntime runtime = prepare_runtime();
    runtime.variables[0] = 201;
    runtime.variables[7] = 208;
    runtime.samples.values[201] = UINT32_C(0x60000000);
    runtime.samples.values[208] = UINT32_C(0x60000007);
    runtime.slots[3].values[0] = 211;
    runtime.slots[3].values[3] = 214;
    runtime.samples.values[211] = UINT32_C(0x70000000);
    runtime.samples.values[214] = UINT32_C(0x70000003);

    const uint8_t script[] = {
        0x20, 0x2b, 0x30, 0x37, 0x40, 0x43, 0x44, 0x47,
        0x48, 0x49, 0x4a, 0x4b, 0x50, 0x57, 0x60, 0x69,
    };
    const uint32_t expected[] = {
        201,
        UINT32_C(0x1000000b),
        UINT32_C(0x60000000),
        UINT32_C(0x60000007),
        211,
        214,
        UINT32_C(0x70000000),
        UINT32_C(0x70000003),
        UINT32_C(0x50000000),
        UINT32_C(0x50000001),
        UINT32_C(0x50000002),
        UINT32_C(0x50000003),
        UINT32_C(0x20000000),
        UINT32_C(0x20000007),
        UINT32_C(0x30000000),
        UINT32_C(0x30000009),
    };
    size_t cursor = 0;
    for (size_t index = 0; index < sizeof(expected) / sizeof(expected[0]); index++) {
        assert(read_operand(&runtime, script, sizeof(script), &cursor) == expected[index]);
    }
    assert(cursor == sizeof(script));
}

static void write_operand(ForceFeedbackScriptRuntime *runtime, const uint8_t *script, size_t length,
                          uint32_t value) {
    size_t cursor = 0;
    ForceFeedbackScriptDestinationResult result =
        force_feedback_script_operand_write(runtime, script, length, cursor, value, true);
    assert(result.valid);
    assert(result.cursor == length);
}

static void test_writes_runtime_banks(void) {
    ForceFeedbackScriptRuntime runtime = prepare_runtime();
    runtime.variables[1] = 90;
    runtime.slots[3].values[2] = 91;

    const uint8_t direct_low[] = {0x14, 7};
    const uint8_t direct_high[] = {0x15, 8};
    const uint8_t variable[] = {0x27};
    const uint8_t variable_sample[] = {0x31};
    const uint8_t slot_value[] = {0x41};
    const uint8_t slot_sample[] = {0x46};
    const uint8_t primary_motion[] = {0x50};
    const uint8_t secondary_motion[] = {0x52};
    const uint8_t axis[] = {0x69};

    write_operand(&runtime, direct_low, sizeof(direct_low), 1);
    write_operand(&runtime, direct_high, sizeof(direct_high), 2);
    write_operand(&runtime, variable, sizeof(variable), 3);
    write_operand(&runtime, variable_sample, sizeof(variable_sample), 4);
    write_operand(&runtime, slot_value, sizeof(slot_value), 5);
    runtime.slots[3].values[2] = 91;
    write_operand(&runtime, slot_sample, sizeof(slot_sample), 6);
    write_operand(&runtime, primary_motion, sizeof(primary_motion), 7);
    write_operand(&runtime, secondary_motion, sizeof(secondary_motion), 8);
    write_operand(&runtime, axis, sizeof(axis), 9);

    assert(runtime.samples.values[7] == 1);
    assert(runtime.samples.values[264] == 2);
    assert(runtime.variables[7] == 3);
    assert(runtime.samples.values[90] == 4);
    assert(runtime.slots[3].values[1] == 5);
    assert(runtime.slots[3].values[2] == 91);
    assert(runtime.samples.values[91] == 6);
    assert(runtime.motion[0] == 7);
    assert(runtime.motion[2] == 8);
    assert(runtime.axes[9] == 9);
}

static void test_accumulates_and_limits_secondary_motion(void) {
    ForceFeedbackScriptRuntime runtime = {0};
    const uint8_t destination[] = {0x53};

    runtime.motion[2] = float_bits(0.75f);
    write_operand(&runtime, destination, sizeof(destination), float_bits(0.5f));
    assert(runtime.motion[2] == float_bits(1.0f));

    runtime.motion[2] = float_bits(-0.75f);
    write_operand(&runtime, destination, sizeof(destination), float_bits(-0.5f));
    assert(runtime.motion[2] == float_bits(-1.0f));

    runtime.motion[2] = float_bits(0.25f);
    write_operand(&runtime, destination, sizeof(destination), float_bits(-0.5f));
    assert(runtime.motion[2] == float_bits(-0.25f));
}

static void test_skips_destinations_without_writing(void) {
    ForceFeedbackScriptRuntime runtime = prepare_runtime();
    ForceFeedbackScriptRuntime before = runtime;
    const uint8_t script[] = {0xff, 0x14, 42};
    size_t cursor = 0;

    ForceFeedbackScriptDestinationResult result =
        force_feedback_script_operand_write(&runtime, script, sizeof(script), cursor, 7, false);
    assert(result.valid);
    assert(result.cursor == 1);
    result = force_feedback_script_operand_write(&runtime, script, sizeof(script), result.cursor, 8,
                                                 false);
    assert(result.valid);
    assert(result.cursor == sizeof(script));
    assert(memcmp(&runtime, &before, sizeof(runtime)) == 0);
}

static void test_rejects_invalid_or_incomplete_operands(void) {
    ForceFeedbackScriptRuntime runtime = {0};
    size_t cursor = 0;
    const uint8_t invalid[] = {0x05};
    ForceFeedbackScriptOperandResult read =
        force_feedback_script_operand_read(&runtime, invalid, sizeof(invalid), cursor);
    assert(!read.valid);
    assert(read.cursor == sizeof(invalid));

    cursor = 0;
    const uint8_t incomplete[] = {0x13, 1, 2, 3};
    read = force_feedback_script_operand_read(&runtime, incomplete, sizeof(incomplete), cursor);
    assert(!read.valid);
    assert(read.cursor == sizeof(incomplete));

    cursor = 0;
    const uint8_t read_only[] = {0x2b};
    ForceFeedbackScriptDestinationResult write = force_feedback_script_operand_write(
        &runtime, read_only, sizeof(read_only), cursor, 1, true);
    assert(!write.valid);
    assert(write.cursor == sizeof(read_only));

    runtime.active_slot = FORCE_FEEDBACK_SCRIPT_SLOT_COUNT;
    cursor = 0;
    const uint8_t slot[] = {0x40};
    read = force_feedback_script_operand_read(&runtime, slot, sizeof(slot), cursor);
    assert(!read.valid);
}

int main(void) {
    test_reads_constants_and_immediates();
    test_reads_samples_and_scaled_literals();
    test_reads_runtime_banks();
    test_writes_runtime_banks();
    test_accumulates_and_limits_secondary_motion();
    test_skips_destinations_without_writing();
    test_rejects_invalid_or_incomplete_operands();
    return 0;
}
