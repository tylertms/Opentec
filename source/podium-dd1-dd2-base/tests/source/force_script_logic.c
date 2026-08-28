#include <assert.h>
#include <math.h>

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

static void test_nan_is_false(void) {
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_AND, NAN, 1.0f) ==
           0.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_OR, NAN, 0.0f) ==
           0.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_NOT, NAN, 1.0f) ==
           1.0f);
    assert(force_feedback_script_logic_evaluate(FORCE_FEEDBACK_SCRIPT_LOGICAL_XNOR, NAN, -0.0f) ==
           1.0f);
}

int main(void) {
    test_binary_truth_tables();
    test_unary_not();
    test_nan_is_false();
    return 0;
}
