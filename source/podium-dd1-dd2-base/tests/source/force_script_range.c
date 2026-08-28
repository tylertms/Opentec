#include <assert.h>
#include <math.h>

#include "force_feedback/script_range.h"

static void test_classifies_range_boundaries(void) {
    assert(force_feedback_script_range_evaluate(FORCE_FEEDBACK_SCRIPT_RANGE_CLASSIFY, 10.0f, 20.0f,
                                                10.0f) == -1.0f);
    assert(force_feedback_script_range_evaluate(FORCE_FEEDBACK_SCRIPT_RANGE_CLASSIFY, 10.0f, 20.0f,
                                                15.0f) == 0.0f);
    assert(force_feedback_script_range_evaluate(FORCE_FEEDBACK_SCRIPT_RANGE_CLASSIFY, 10.0f, 20.0f,
                                                20.0f) == 1.0f);
}

static void test_classification_uses_ordered_comparisons(void) {
    assert(force_feedback_script_range_evaluate(FORCE_FEEDBACK_SCRIPT_RANGE_CLASSIFY, NAN, 20.0f,
                                                21.0f) == 1.0f);
    assert(force_feedback_script_range_evaluate(FORCE_FEEDBACK_SCRIPT_RANGE_CLASSIFY, 10.0f, NAN,
                                                21.0f) == 0.0f);
    assert(force_feedback_script_range_evaluate(FORCE_FEEDBACK_SCRIPT_RANGE_CLASSIFY, 10.0f, 20.0f,
                                                NAN) == 0.0f);
}

static void test_normalizes_between_boundaries(void) {
    assert(force_feedback_script_range_evaluate(FORCE_FEEDBACK_SCRIPT_RANGE_NORMALIZE, 10.0f, 20.0f,
                                                15.0f) == 0.5f);
    assert(force_feedback_script_range_evaluate(FORCE_FEEDBACK_SCRIPT_RANGE_NORMALIZE, 10.0f, 20.0f,
                                                25.0f) == 1.5f);
}

static void test_bounded_normalization_uses_range_results(void) {
    assert(force_feedback_script_range_evaluate(FORCE_FEEDBACK_SCRIPT_RANGE_NORMALIZE_BOUNDED,
                                                10.0f, 20.0f, 10.0f) == -1.0f);
    assert(force_feedback_script_range_evaluate(FORCE_FEEDBACK_SCRIPT_RANGE_NORMALIZE_BOUNDED,
                                                10.0f, 20.0f, 15.0f) == 0.5f);
    assert(force_feedback_script_range_evaluate(FORCE_FEEDBACK_SCRIPT_RANGE_NORMALIZE_BOUNDED,
                                                10.0f, 20.0f, 20.0f) == 1.0f);
    assert(isnan(force_feedback_script_range_evaluate(FORCE_FEEDBACK_SCRIPT_RANGE_NORMALIZE_BOUNDED,
                                                      10.0f, 20.0f, NAN)));
}

int main(void) {
    test_classifies_range_boundaries();
    test_classification_uses_ordered_comparisons();
    test_normalizes_between_boundaries();
    test_bounded_normalization_uses_range_results();
    return 0;
}
