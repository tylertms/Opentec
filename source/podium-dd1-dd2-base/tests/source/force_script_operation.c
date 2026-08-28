#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "force_feedback/script_bits.h"
#include "force_feedback/script_compare.h"
#include "force_feedback/script_integer.h"
#include "force_feedback/script_logic.h"
#include "force_feedback/script_math.h"
#include "force_feedback/script_operation.h"
#include "force_feedback/script_range.h"

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void execute(ForceFeedbackScriptRuntime *runtime, uint8_t operation, const uint8_t *script,
                    size_t length) {
    ForceFeedbackScriptDestinationResult result =
        force_feedback_script_operation_execute(runtime, operation, script, length, 0, true);
    assert(result.valid);
    assert(result.cursor == length);
}

static void test_executes_float_operation_groups(void) {
    ForceFeedbackScriptRuntime runtime = {0};
    const uint8_t add[] = {
        0x13, 0x3f, 0x80, 0x00, 0x00, 0x13, 0x40, 0x00, 0x00, 0x00, 0x20,
    };
    execute(&runtime, FORCE_FEEDBACK_SCRIPT_MATH_ADD, add, sizeof(add));
    assert(runtime.variables[0] == float_bits(3.0f));

    const uint8_t logical_not[] = {0x00, 0x21};
    execute(&runtime, FORCE_FEEDBACK_SCRIPT_LOGICAL_NOT, logical_not, sizeof(logical_not));
    assert(runtime.variables[1] == float_bits(1.0f));

    const uint8_t negative[] = {0x13, 0xbf, 0x80, 0x00, 0x00, 0x22};
    execute(&runtime, FORCE_FEEDBACK_SCRIPT_NEGATIVE, negative, sizeof(negative));
    assert(runtime.variables[2] == float_bits(1.0f));

    const uint8_t magnitude[] = {
        0x13, 0x40, 0x40, 0x00, 0x00, 0x13, 0x40, 0x80, 0x00, 0x00, 0x23,
    };
    execute(&runtime, FORCE_FEEDBACK_SCRIPT_MATH_VECTOR_MAGNITUDE, magnitude, sizeof(magnitude));
    assert(runtime.variables[3] == float_bits(5.0f));
}

static void test_executes_bit_operations(void) {
    ForceFeedbackScriptRuntime runtime = {0};
    const uint8_t set_bit[] = {0x20, 0x10, 7};
    execute(&runtime, FORCE_FEEDBACK_SCRIPT_SET_BIT, set_bit, sizeof(set_bit));
    assert(runtime.variables[0] == UINT32_C(0x80));

    const uint8_t test_bit[] = {0x20, 0x10, 7, 0x21};
    execute(&runtime, FORCE_FEEDBACK_SCRIPT_TEST_BIT, test_bit, sizeof(test_bit));
    assert(runtime.variables[1] == float_bits(1.0f));

    const uint8_t clear_bit[] = {0x20, 0x10, 7};
    execute(&runtime, FORCE_FEEDBACK_SCRIPT_CLEAR_BIT, clear_bit, sizeof(clear_bit));
    assert(runtime.variables[0] == 0);
}

static void test_executes_copy_and_sample_operations(void) {
    ForceFeedbackScriptRuntime runtime = {0};
    runtime.samples.values[100] = UINT32_C(0xaaaa0000);
    runtime.samples.values[102] = UINT32_C(0xaaaa0002);
    runtime.samples.values[109] = UINT32_C(0xaaaa0009);

    const uint8_t copy[] = {0x14, 100, 0x20};
    execute(&runtime, 0xa0, copy, sizeof(copy));
    assert(runtime.variables[0] == UINT32_C(0xaaaa0000));

    const uint8_t sample[] = {0x14, 100, 0x10, 2, 0x21};
    execute(&runtime, 0xa1, sample, sizeof(sample));
    assert(runtime.variables[1] == UINT32_C(0xaaaa0002));

    const uint8_t wrapped[] = {0x14, 100, 0x10, 25, 0x10, 16, 0x22};
    execute(&runtime, 0xa2, wrapped, sizeof(wrapped));
    assert(runtime.variables[2] == UINT32_C(0xaaaa0009));
}

static void test_executes_range_and_integer_operations(void) {
    ForceFeedbackScriptRuntime runtime = {0};
    const uint8_t normalize[] = {
        0x00, 0x13, 0x40, 0x00, 0x00, 0x00, 0x13, 0x3f, 0x80, 0x00, 0x00, 0x20,
    };
    execute(&runtime, FORCE_FEEDBACK_SCRIPT_RANGE_NORMALIZE, normalize, sizeof(normalize));
    assert(runtime.variables[0] == float_bits(0.5f));

    const uint8_t u32_to_float[] = {0x10, 42, 0x21};
    execute(&runtime, FORCE_FEEDBACK_SCRIPT_INTEGER_U32_TO_FLOAT, u32_to_float,
            sizeof(u32_to_float));
    assert(runtime.variables[1] == float_bits(42.0f));

    const uint8_t absolute_difference[] = {0x10, 9, 0x10, 20, 0x22};
    execute(&runtime, FORCE_FEEDBACK_SCRIPT_INTEGER_ABSOLUTE_DIFFERENCE, absolute_difference,
            sizeof(absolute_difference));
    assert(runtime.variables[2] == 11);
}

static void test_scales_rotation_from_runtime_range(void) {
    ForceFeedbackScriptRuntime runtime = {
        .extended_rotation_range = 1080,
        .rotation_range_code = 126,
    };
    const uint8_t script[] = {0x02, 0x20};
    execute(&runtime, 0xd7, script, sizeof(script));
    assert(runtime.variables[0] == float_bits(1.0f * 3.1415927f * 5400.0f / 180.0f));
}

static void test_consumes_without_committing(void) {
    ForceFeedbackScriptRuntime runtime = {0};
    const uint8_t script[] = {0x01, 0x01, 0x20};
    ForceFeedbackScriptDestinationResult result = force_feedback_script_operation_execute(
        &runtime, FORCE_FEEDBACK_SCRIPT_MATH_ADD, script, sizeof(script), 0, false);
    assert(result.valid);
    assert(result.cursor == sizeof(script));
    assert(runtime.variables[0] == 0);
}

static void test_rejects_invalid_records(void) {
    ForceFeedbackScriptRuntime runtime = {0};
    const uint8_t divide_by_zero[] = {0x02, 0x00, 0x20};
    ForceFeedbackScriptDestinationResult result =
        force_feedback_script_operation_execute(&runtime, FORCE_FEEDBACK_SCRIPT_MATH_DIVIDE,
                                                divide_by_zero, sizeof(divide_by_zero), 0, true);
    assert(!result.valid);

    const uint8_t invalid_bit[] = {0x20, 0x10, 32};
    result = force_feedback_script_operation_execute(&runtime, FORCE_FEEDBACK_SCRIPT_SET_BIT,
                                                     invalid_bit, sizeof(invalid_bit), 0, true);
    assert(!result.valid);

    const uint8_t incomplete[] = {0x01};
    result = force_feedback_script_operation_execute(&runtime, FORCE_FEEDBACK_SCRIPT_MATH_ADD,
                                                     incomplete, sizeof(incomplete), 0, true);
    assert(!result.valid);
}

int main(void) {
    test_executes_float_operation_groups();
    test_executes_bit_operations();
    test_executes_copy_and_sample_operations();
    test_executes_range_and_integer_operations();
    test_scales_rotation_from_runtime_range();
    test_consumes_without_committing();
    test_rejects_invalid_records();
    return 0;
}
