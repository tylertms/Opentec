#include <assert.h>
#include <stdint.h>

#include "force_feedback/script_input.h"
#include "force_feedback/script_sample.h"

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

int main(void) {
    test_reads_base_relative_sample();
    test_rejects_sample_index_above_table();
    test_reads_wrapped_sample();
    test_rejects_invalid_wrapped_read();
    return 0;
}
