#include <assert.h>
#include <math.h>

#include "force_feedback/script_compare.h"

static void test_binary_comparisons(void) {
    assert(force_feedback_script_compare(FORCE_FEEDBACK_SCRIPT_GREATER_THAN, 2.0f, 1.0f) == 1.0f);
    assert(force_feedback_script_compare(FORCE_FEEDBACK_SCRIPT_GREATER_THAN, 2.0f, 2.0f) == 0.0f);
    assert(force_feedback_script_compare(FORCE_FEEDBACK_SCRIPT_GREATER_OR_EQUAL, 2.0f, 2.0f) ==
           1.0f);
    assert(force_feedback_script_compare(FORCE_FEEDBACK_SCRIPT_GREATER_OR_EQUAL, 1.0f, 2.0f) ==
           0.0f);
    assert(force_feedback_script_compare(FORCE_FEEDBACK_SCRIPT_LESS_THAN, 1.0f, 2.0f) == 1.0f);
    assert(force_feedback_script_compare(FORCE_FEEDBACK_SCRIPT_LESS_THAN, 2.0f, 2.0f) == 0.0f);
    assert(force_feedback_script_compare(FORCE_FEEDBACK_SCRIPT_LESS_OR_EQUAL, 2.0f, 2.0f) == 1.0f);
    assert(force_feedback_script_compare(FORCE_FEEDBACK_SCRIPT_LESS_OR_EQUAL, 2.0f, 1.0f) == 0.0f);
}

static void test_sign_comparisons(void) {
    assert(force_feedback_script_compare(FORCE_FEEDBACK_SCRIPT_NEGATIVE, -1.0f, 99.0f) == 1.0f);
    assert(force_feedback_script_compare(FORCE_FEEDBACK_SCRIPT_NEGATIVE, -0.0f, 99.0f) == 0.0f);
    assert(force_feedback_script_compare(FORCE_FEEDBACK_SCRIPT_POSITIVE, 1.0f, 99.0f) == 1.0f);
    assert(force_feedback_script_compare(FORCE_FEEDBACK_SCRIPT_POSITIVE, 0.0f, 99.0f) == 0.0f);
}

static void test_nan_comparisons(void) {
    for (ForceFeedbackScriptComparison comparison = FORCE_FEEDBACK_SCRIPT_GREATER_THAN;
         comparison <= FORCE_FEEDBACK_SCRIPT_POSITIVE; comparison++) {
        assert(force_feedback_script_compare(comparison, NAN, NAN) == 0.0f);
    }
    assert(force_feedback_script_compare(FORCE_FEEDBACK_SCRIPT_GREATER_OR_EQUAL, 1.0f, NAN) ==
           0.0f);
    assert(force_feedback_script_compare(FORCE_FEEDBACK_SCRIPT_LESS_OR_EQUAL, 1.0f, NAN) == 0.0f);
}

int main(void) {
    test_binary_comparisons();
    test_sign_comparisons();
    test_nan_comparisons();
    return 0;
}
