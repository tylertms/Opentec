#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "force_feedback/script_range.h"

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

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

static void test_scales_rotation_from_raw_sensitivity_code(void) {
    assert(float_bits(force_feedback_script_rotation_scale(1.0f, 36, 0)) ==
           UINT32_C(0x40490fdb));
    assert(float_bits(force_feedback_script_rotation_scale(1.0f, 126, 36)) == UINT32_C(0x40490fdb));
    assert(float_bits(force_feedback_script_rotation_scale(1.0f, 127, 36)) == UINT32_C(0x40490fdb));
    assert(force_feedback_script_rotation_scale(10.0f, 0, 999) == 0.0f);
}

static void test_uses_signed_sensitivity_boundaries(void) {
    const uint16_t extended_range = 999;

    assert(float_bits(force_feedback_script_rotation_scale(1.0f, 125, extended_range)) ==
           UINT32_C(0x412e886e));
    assert(float_bits(force_feedback_script_rotation_scale(1.0f, 126, extended_range)) ==
           UINT32_C(0x42ae5bc0));
    assert(float_bits(force_feedback_script_rotation_scale(1.0f, 127, extended_range)) ==
           UINT32_C(0x42ae5bc0));
    assert(float_bits(force_feedback_script_rotation_scale(1.0f, 0x80, extended_range)) ==
           UINT32_C(0xc132b8c3));
    assert(float_bits(force_feedback_script_rotation_scale(1.0f, 0xa5, extended_range)) ==
           UINT32_C(0xc0fe1eb5));
}

int main(void) {
    test_classifies_range_boundaries();
    test_classification_uses_ordered_comparisons();
    test_normalizes_between_boundaries();
    test_bounded_normalization_uses_range_results();
    test_scales_rotation_from_raw_sensitivity_code();
    test_uses_signed_sensitivity_boundaries();
    return 0;
}
