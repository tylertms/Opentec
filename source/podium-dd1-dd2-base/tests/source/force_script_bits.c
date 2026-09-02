#include <assert.h>
#include <stdint.h>

#include "force_feedback/script_bits.h"

static void assert_value(ForceFeedbackScriptBitOperation operation, uint32_t first, uint32_t second,
                         uint32_t expected) {
    ForceFeedbackScriptBitResult result =
        force_feedback_script_bits_evaluate(operation, first, second);
    assert(result.writes_value);
    assert(result.value == expected);
}

static void test_bitwise_operations(void) {
    assert_value(FORCE_FEEDBACK_SCRIPT_BITWISE_AND, 0x12345678, 0xff00ff00, 0x12005600);
    assert_value(FORCE_FEEDBACK_SCRIPT_BITWISE_OR, 0x12345678, 0xff00ff00, 0xff34ff78);
    assert_value(FORCE_FEEDBACK_SCRIPT_BITWISE_NAND, 0x12345678, 0xff00ff00, 0xedffa9ff);
    assert_value(FORCE_FEEDBACK_SCRIPT_BITWISE_NOR, 0x12345678, 0xff00ff00, 0x00cb0087);
    assert_value(FORCE_FEEDBACK_SCRIPT_BITWISE_XOR, 0x12345678, 0xff00ff00, 0xed34a978);
    assert_value(FORCE_FEEDBACK_SCRIPT_BITWISE_NOT, 0x12345678, 0, 0xedcba987);
    assert_value(FORCE_FEEDBACK_SCRIPT_BITWISE_XNOR, 0x12345678, 0xff00ff00, 0x12cb5687);
}

static void test_bit_access(void) {
    assert_value(FORCE_FEEDBACK_SCRIPT_TEST_BIT, UINT32_C(0x80000001), 0, UINT32_C(0x3f800000));
    assert_value(FORCE_FEEDBACK_SCRIPT_TEST_BIT, UINT32_C(0x80000001), 30, 0);
    assert_value(FORCE_FEEDBACK_SCRIPT_TEST_BIT, UINT32_C(0x80000001), 31, 0);
    assert_value(FORCE_FEEDBACK_SCRIPT_TEST_BIT, UINT32_C(0x00008000), 15, UINT32_C(0x3f800000));
    assert_value(FORCE_FEEDBACK_SCRIPT_TEST_BIT, UINT32_C(0x80000000), 15, UINT32_C(0x3f800000));
    assert_value(FORCE_FEEDBACK_SCRIPT_TEST_BIT, UINT32_C(0x00010000), 16, 0);
    assert_value(FORCE_FEEDBACK_SCRIPT_SET_BIT, 0, 0, 1);
    assert_value(FORCE_FEEDBACK_SCRIPT_SET_BIT, 1, 31, UINT32_C(0x80000001));
    assert_value(FORCE_FEEDBACK_SCRIPT_CLEAR_BIT, UINT32_MAX, 0, UINT32_C(0xfffffffe));
    assert_value(FORCE_FEEDBACK_SCRIPT_CLEAR_BIT, UINT32_MAX, 31, UINT32_C(0x7fffffff));
}

static void test_invalid_bit_indexes_skip_write(void) {
    assert(!force_feedback_script_bits_evaluate(FORCE_FEEDBACK_SCRIPT_TEST_BIT, UINT32_MAX, 32)
                .writes_value);
    assert(!force_feedback_script_bits_evaluate(FORCE_FEEDBACK_SCRIPT_SET_BIT, 0, 32).writes_value);
    assert(!force_feedback_script_bits_evaluate(FORCE_FEEDBACK_SCRIPT_CLEAR_BIT, UINT32_MAX,
                                                UINT32_MAX)
                .writes_value);
}

int main(void) {
    test_bitwise_operations();
    test_bit_access();
    test_invalid_bit_indexes_skip_write();
    return 0;
}
