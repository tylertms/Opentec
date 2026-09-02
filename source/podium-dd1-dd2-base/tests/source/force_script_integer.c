#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "force_feedback/script_integer.h"

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void assert_value(ForceFeedbackScriptIntegerOperation operation, uint32_t first,
                         uint32_t second, uint32_t expected) {
    ForceFeedbackScriptIntegerResult result =
        force_feedback_script_integer_evaluate(operation, first, second);
    assert(result.writes_value);
    assert(result.value == expected);
}

static void test_converts_unsigned_integer_to_float(void) {
    assert_value(FORCE_FEEDBACK_SCRIPT_INTEGER_U32_TO_FLOAT, 42, 0, float_bits(42.0f));
    assert_value(FORCE_FEEDBACK_SCRIPT_INTEGER_U32_TO_FLOAT, UINT32_MAX, 0, UINT32_C(0x4f800000));
}

static void test_converts_bounded_float_to_unsigned_integer(void) {
    assert_value(FORCE_FEEDBACK_SCRIPT_INTEGER_FLOAT_TO_U32, float_bits(42.75f), 0, 42);
    assert_value(FORCE_FEEDBACK_SCRIPT_INTEGER_FLOAT_TO_U32, float_bits(-1.0f), 0, UINT32_MAX);
    assert_value(FORCE_FEEDBACK_SCRIPT_INTEGER_FLOAT_TO_U32, UINT32_C(0x7fc00000), 0, 0);
    assert_value(FORCE_FEEDBACK_SCRIPT_INTEGER_FLOAT_TO_U32, UINT32_C(0xffc00000), 0, 0);
    assert_value(FORCE_FEEDBACK_SCRIPT_INTEGER_FLOAT_TO_U32, UINT32_C(0x4dcccccd), 0,
                 UINT32_C(429496736));
    assert(!force_feedback_script_integer_evaluate(FORCE_FEEDBACK_SCRIPT_INTEGER_FLOAT_TO_U32,
                                                   UINT32_C(0x4dccccce), 0)
                .writes_value);
}

static void test_converts_signed_difference_to_float(void) {
    assert_value(FORCE_FEEDBACK_SCRIPT_INTEGER_SUBTRACT_I32_TO_FLOAT, (uint32_t)-5, 3,
                 float_bits(-8.0f));
}

static void test_calculates_unsigned_absolute_difference(void) {
    assert_value(FORCE_FEEDBACK_SCRIPT_INTEGER_ABSOLUTE_DIFFERENCE, 5, 12, 7);
    assert_value(FORCE_FEEDBACK_SCRIPT_INTEGER_ABSOLUTE_DIFFERENCE, UINT32_MAX, 0, UINT32_MAX);
}

static void test_calculates_unsigned_modulo(void) {
    assert_value(FORCE_FEEDBACK_SCRIPT_INTEGER_MODULO_TO_FLOAT, 17, 5, float_bits(2.0f));
    assert_value(FORCE_FEEDBACK_SCRIPT_INTEGER_MODULO, 17, 5, 2);
    assert(!force_feedback_script_integer_evaluate(FORCE_FEEDBACK_SCRIPT_INTEGER_MODULO_TO_FLOAT,
                                                   17, 0)
                .writes_value);
    assert(!force_feedback_script_integer_evaluate(FORCE_FEEDBACK_SCRIPT_INTEGER_MODULO, 17, 0)
                .writes_value);
}

static void test_converts_unsigned_degrees_to_radians(void) {
    assert_value(FORCE_FEEDBACK_SCRIPT_INTEGER_DEGREES_TO_RADIANS, 180, 0, UINT32_C(0x40490fdb));
}

int main(void) {
    test_converts_unsigned_integer_to_float();
    test_converts_bounded_float_to_unsigned_integer();
    test_converts_signed_difference_to_float();
    test_calculates_unsigned_absolute_difference();
    test_calculates_unsigned_modulo();
    test_converts_unsigned_degrees_to_radians();
    return 0;
}
