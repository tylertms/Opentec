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
    assert(force_feedback_script_output_request(float_bits(1.0f), 101, 35) == 22937);
    assert(force_feedback_script_output_request(float_bits(1.0f), 101, 30) == 19660);
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

static ForceFeedbackScriptOutputConfig full_output_config(void) {
    return (ForceFeedbackScriptOutputConfig){
        .soft_stop = {.travel_limit = 2000},
        .available_percent = 100,
        .output_strength_percent = 100,
        .automatic_strength = 100,
        .ramp_percent = 100,
        .smoothing_intensity = 100,
        .tuning_strength = 100,
    };
}

static void test_applies_complete_motor_output_pipeline(void) {
    ForceFeedbackScriptOutputState state;
    force_feedback_script_output_init(&state);
    ForceFeedbackScriptOutputConfig config = full_output_config();
    ForceOutputReport report = {0};

    assert(!force_feedback_script_output_apply(&state, float_bits(1.0f), 0, 0, &config, &report));
    assert(report.positive_direction);
    assert(report.primary_magnitude == 0);

    assert(!force_feedback_script_output_apply(&state, float_bits(1.0f), 0, 1, &config, &report));
    assert(report.primary_magnitude == UINT16_MAX);
}

static void test_applies_position_limit_and_secondary_gate(void) {
    ForceFeedbackScriptOutputState state;
    force_feedback_script_output_init(&state);
    ForceFeedbackScriptOutputConfig config = full_output_config();
    ForceOutputReport report = {.secondary_magnitude = 0x4321};

    force_feedback_script_output_apply(&state, 0, 0, 0, &config, &report);
    force_feedback_script_output_apply(&state, 0, 0, 1, &config, &report);
    state.soft_stop.ramp_percent = 100;

    assert(force_feedback_script_output_apply(&state, 0, 3500, 2, &config, &report));
    assert(report.positive_direction);
    assert(report.primary_magnitude == 4500);

    config.secondary_output_disabled = true;
    report.secondary_magnitude = 0x4321;
    assert(!force_feedback_script_output_apply(&state, 0, 3500, 3, &config, &report));
    assert(report.primary_magnitude == 0);
    assert(report.secondary_magnitude == 0x4321);
}

int main(void) {
    test_scales_normal_and_automatic_strength();
    test_scales_extended_strength_and_sentinel();
    test_applies_post_filter_ramp();
    test_applies_complete_motor_output_pipeline();
    test_applies_position_limit_and_secondary_gate();
    return 0;
}
