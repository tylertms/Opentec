#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "force_feedback/script_input.h"
#include "force_feedback/script_sample.h"

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static ForceFeedbackScriptSamples prepare_samples(void) {
    ForceFeedbackScriptSamples samples = {0};
    for (uint16_t index = 0; index < FORCE_FEEDBACK_SCRIPT_SAMPLE_COUNT; index++) {
        samples.values[index] = UINT32_C(0x10000000) + index;
    }
    return samples;
}

static void test_reads_base_relative_sample(void) {
    ForceFeedbackScriptSamples samples = prepare_samples();
    ForceFeedbackScriptSampleResult result = force_feedback_script_sample_read(&samples, 100, 23);
    assert(result.writes_value);
    assert(result.value == UINT32_C(0x1000007b));

    result = force_feedback_script_sample_read(&samples, UINT32_MAX, 1);
    assert(result.writes_value);
    assert(result.value == UINT32_C(0x10000000));
}

static void test_rejects_sample_index_above_table(void) {
    ForceFeedbackScriptSamples samples = prepare_samples();
    assert(!force_feedback_script_sample_read(&samples, 511, 1).writes_value);
    assert(!force_feedback_script_sample_read(&samples, 512, 0).writes_value);
}

static void test_reads_wrapped_sample(void) {
    ForceFeedbackScriptSamples samples = prepare_samples();
    ForceFeedbackScriptSampleResult result =
        force_feedback_script_sample_read_wrapped(&samples, 200, 37, 16);
    assert(result.writes_value);
    assert(result.value == UINT32_C(0x100000cd));

    result = force_feedback_script_sample_read_wrapped(&samples, UINT32_MAX, 16, 15);
    assert(result.writes_value);
    assert(result.value == UINT32_C(0x10000000));
}

static void test_rejects_invalid_wrapped_read(void) {
    ForceFeedbackScriptSamples samples = prepare_samples();
    assert(!force_feedback_script_sample_read_wrapped(&samples, 0, 1, 0).writes_value);
    assert(!force_feedback_script_sample_read_wrapped(&samples, 511, 1, 2).writes_value);
}

static ForceFeedbackScriptSamples prepare_curve(void) {
    ForceFeedbackScriptSamples samples = {0};
    samples.values[100] = float_bits(0.0f);
    samples.values[101] = float_bits(0.0f);
    samples.values[102] = float_bits(10.0f);
    samples.values[103] = float_bits(100.0f);
    samples.values[104] = float_bits(20.0f);
    samples.values[105] = float_bits(400.0f);
    return samples;
}

static void assert_interpolation(float target, float expected) {
    ForceFeedbackScriptSamples samples = prepare_curve();
    ForceFeedbackScriptSampleResult result =
        force_feedback_script_sample_interpolate(&samples, 100, 3, target);
    assert(result.writes_value);
    assert(result.value == float_bits(expected));
}

static void test_interpolates_and_extrapolates_curve(void) {
    assert_interpolation(-5.0f, -50.0f);
    assert_interpolation(0.0f, 0.0f);
    assert_interpolation(5.0f, 50.0f);
    assert_interpolation(20.0f, 400.0f);
    assert_interpolation(25.0f, 550.0f);
}

static void test_rejects_invalid_curve(void) {
    ForceFeedbackScriptSamples samples = prepare_curve();
    assert(!force_feedback_script_sample_interpolate(&samples, 100, 1, 5.0f).writes_value);
    assert(!force_feedback_script_sample_interpolate(&samples, 510, 2, 5.0f).writes_value);
    assert(!force_feedback_script_sample_interpolate(&samples, 100, 3, NAN).writes_value);
}

int main(void) {
    test_reads_base_relative_sample();
    test_rejects_sample_index_above_table();
    test_reads_wrapped_sample();
    test_rejects_invalid_wrapped_read();
    test_interpolates_and_extrapolates_curve();
    test_rejects_invalid_curve();
    return 0;
}
