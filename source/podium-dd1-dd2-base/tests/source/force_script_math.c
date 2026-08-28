#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "force_feedback/script_math.h"

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void assert_value(ForceFeedbackScriptMathOperation operation, float first, float second,
                         float expected) {
    ForceFeedbackScriptMathResult result =
        force_feedback_script_math_evaluate(operation, first, second);
    assert(result.writes_value);
    assert(result.value == expected);
}

static void test_binary_operations(void) {
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_ADD, 7.0f, 3.0f, 10.0f);
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_SUBTRACT, 7.0f, 3.0f, 4.0f);
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_MULTIPLY, 7.0f, 3.0f, 21.0f);
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_DIVIDE, 12.0f, 3.0f, 4.0f);
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_MODULO, 5.0f, 3.0f, 2.0f);
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_MODULO, -5.0f, 3.0f, 1.0f);
}

static void test_unary_operations(void) {
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_SQUARE, -3.0f, 99.0f, 9.0f);
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_CUBE, -2.0f, 99.0f, -8.0f);
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_SQUARE_ROOT, 16.0f, 99.0f, 4.0f);
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_SIGN, 4.0f, 99.0f, 1.0f);
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_SIGN, 0.0f, 99.0f, 0.0f);
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_SIGN, -4.0f, 99.0f, -1.0f);
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_RECIPROCAL, 4.0f, 99.0f, 0.25f);

    ForceFeedbackScriptMathResult absolute =
        force_feedback_script_math_evaluate(FORCE_FEEDBACK_SCRIPT_MATH_ABSOLUTE, -0.0f, 99.0f);
    assert(absolute.writes_value);
    assert(float_bits(absolute.value) == 0);
}

static void test_trigonometric_operations(void) {
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_SINE, 0.0f, 99.0f, 0.0f);
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_COSINE, 0.0f, 99.0f, 1.0f);
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_TANGENT, 0.0f, 99.0f, 0.0f);

    ForceFeedbackScriptMathResult multiplied =
        force_feedback_script_math_evaluate(FORCE_FEEDBACK_SCRIPT_MATH_MULTIPLY_PI, 1.0f, 99.0f);
    assert(multiplied.writes_value);
    assert(float_bits(multiplied.value) == UINT32_C(0x40490fdb));
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_DIVIDE_PI, multiplied.value, 99.0f, 1.0f);
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_DEGREES_TO_RADIANS, 180.0f, 99.0f, multiplied.value);
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_RADIANS_TO_DEGREES, multiplied.value, 99.0f, 180.0f);
}

static void test_vector_operations(void) {
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_VECTOR_MAGNITUDE, 3.0f, 4.0f, 5.0f);
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_MULTIPLY_COSINE, 7.0f, 0.0f, 7.0f);
    assert_value(FORCE_FEEDBACK_SCRIPT_MATH_MULTIPLY_SINE, 7.0f, 0.0f, 0.0f);
}

static void test_skipped_writes(void) {
    assert(!force_feedback_script_math_evaluate(FORCE_FEEDBACK_SCRIPT_MATH_DIVIDE, 1.0f, 0.0f)
                .writes_value);
    assert(!force_feedback_script_math_evaluate(FORCE_FEEDBACK_SCRIPT_MATH_MODULO, 1.0f, -0.0f)
                .writes_value);
    assert(!force_feedback_script_math_evaluate(FORCE_FEEDBACK_SCRIPT_MATH_SQUARE_ROOT, -1.0f, 0.0f)
                .writes_value);
    assert(!force_feedback_script_math_evaluate(FORCE_FEEDBACK_SCRIPT_MATH_RECIPROCAL, -0.0f, 0.0f)
                .writes_value);
    assert(!force_feedback_script_math_evaluate(FORCE_FEEDBACK_SCRIPT_MATH_TANGENT, NAN, 0.0f)
                .writes_value);
    assert(!force_feedback_script_math_evaluate(0xff, 1.0f, 2.0f).writes_value);
}

int main(void) {
    test_binary_operations();
    test_unary_operations();
    test_trigonometric_operations();
    test_vector_operations();
    test_skipped_writes();
    return 0;
}
