#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "force_feedback/script_logic.h"

static void test_binary_truth_tables(void) {
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_AND, 2.0f, -3.0f) ==
           1.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_AND, 2.0f, 0.0f) ==
           0.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_OR, 0.0f, -3.0f) ==
           1.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_OR, -0.0f, 0.0f) ==
           0.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_NAND, 2.0f, -3.0f) ==
           0.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_NAND, 2.0f, 0.0f) ==
           1.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_NOR, 0.0f, -0.0f) ==
           1.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_NOR, 0.0f, 4.0f) ==
           0.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_XOR, 4.0f, 0.0f) ==
           1.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_XOR, 4.0f, -4.0f) ==
           0.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_XNOR, 4.0f, -4.0f) ==
           1.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_XNOR, 4.0f, 0.0f) ==
           0.0f);
}

static void test_unary_not(void) {
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_NOT, 0.0f, 8.0f) ==
           1.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_NOT, -2.0f, 0.0f) ==
           0.0f);
}

static float bits_float(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void test_nan_is_true(void) {
    float quiet_nan = bits_float(UINT32_C(0x7fc00001));
    float signaling_nan = bits_float(UINT32_C(0x7fa00001));
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_AND, quiet_nan,
                                                1.0f) == 1.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_OR, quiet_nan,
                                                0.0f) == 1.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_NAND, quiet_nan,
                                                1.0f) == 0.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_NOR, quiet_nan,
                                                0.0f) == 0.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_XOR, quiet_nan,
                                                0.0f) == 1.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_NOT, quiet_nan,
                                                1.0f) == 0.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_XNOR, quiet_nan,
                                                0.0f) == 0.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_OR, signaling_nan,
                                                0.0f) == 1.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_NOT, signaling_nan,
                                                0.0f) == 0.0f);
}

int main(void) {
    test_binary_truth_tables();
    test_unary_not();
    test_nan_is_true();
    return 0;
}
