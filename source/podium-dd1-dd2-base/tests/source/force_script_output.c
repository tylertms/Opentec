#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "force_feedback/script_output.h"

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void test_scales_normal_and_automatic_strength(void) {
    assert(force_feedback_script_output_request(float_bits(1.0f), 100, 0) == 65535);
    assert(force_feedback_script_output_request(float_bits(1.0f), 50, 0) == 32767);
    assert(force_feedback_script_output_request(float_bits(-1.0f), 100, 0) == -65535);
    assert(force_feedback_script_output_request(float_bits(1.0f), 101, 80) == 52428);
}

static void test_scales_extended_strength_and_sentinel(void) {
    assert(force_feedback_script_output_request(float_bits(1.0f), 110, 0) == 124516);
    assert(force_feedback_script_output_request(UINT32_MAX, 100, 100) == 0);
}

static void test_applies_post_filter_ramp(void) {
    assert(force_feedback_script_output_ramp(65535, 50) == 32767);
    assert(force_feedback_script_output_ramp(-65535, 50) == -32767);
    assert(force_feedback_script_output_ramp(12345, 0) == 0);
}

int main(void) {
    test_scales_normal_and_automatic_strength();
    test_scales_extended_strength_and_sentinel();
    test_applies_post_filter_ramp();
    return 0;
}
