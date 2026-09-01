#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "force_feedback/script_tick.h"

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static ForceFeedbackScriptSystem prepare_system(const uint8_t *script, uint16_t length) {
    ForceFeedbackScriptSystem system;
    force_feedback_script_runtime_init(&system);
    system.mode = FORCE_FEEDBACK_RUNTIME_ACTIVE;
    system.inputs.status = FORCE_FEEDBACK_SCRIPT_INPUT_READY;
    system.inputs.deadline = 100;
    system.inputs.sample_count = 5;
    system.values.rotation_range_code = 36;
    system.values.slots[0].state = FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE;
    system.store.position_request_pending = false;
    system.store.slots[0] =
        (ForceFeedbackScriptStorageSlot){.offset = 0, .size = length, .allocated = true};
    system.store.used = length;
    memcpy(system.store.data, script, length);
    return system;
}

static void test_runs_host_tick_and_exposes_motion(void) {
    const uint8_t script[] = {0xa0, 0x02, 0x52};
    ForceFeedbackScriptSystem system = prepare_system(script, sizeof(script));
    system.clock.ticks = 20;
    system.clock.motion_ticks = 20;

    ForceFeedbackScriptTickDecision decision = force_feedback_script_tick(&system, 1, 500, 1000);
    assert(decision.output_policy == FORCE_FEEDBACK_SCRIPT_OUTPUT_MOTION);
    assert(!decision.slot_faulted);
    assert(system.scheduler.deadline == 6);
    assert(system.values.variables[8] == float_bits(0.002f));
    assert(system.values.variables[9] == float_bits(0.002f));
    assert(system.values.variables[10] == 1);
    assert(system.values.variables[11] == 20);
    assert(system.values.motion[2] == float_bits(1.0f));
    assert(system.values.motion[4] == float_bits(0.5f));
    assert(system.values.slots[0].execution_count == 1);
    assert(force_feedback_script_tick(&system, 2, 500, 1000).output_policy ==
           FORCE_FEEDBACK_SCRIPT_OUTPUT_NONE);
}

static void test_selects_zero_for_active_expired_and_suppressed_output(void) {
    const uint8_t script[] = {0x00};
    ForceFeedbackScriptSystem active = prepare_system(script, sizeof(script));
    active.inputs.status = FORCE_FEEDBACK_SCRIPT_INPUT_ACTIVE;
    active.clock.motion_ticks = 10;
    assert(force_feedback_script_tick(&active, 1, 0, 1000).output_policy ==
           FORCE_FEEDBACK_SCRIPT_OUTPUT_ZERO);

    ForceFeedbackScriptSystem expired = prepare_system(script, sizeof(script));
    expired.inputs.deadline = 0;
    assert(force_feedback_script_tick(&expired, 1, 0, 1000).output_policy ==
           FORCE_FEEDBACK_SCRIPT_OUTPUT_NONE);
    expired.inputs.deadline = 1;
    expired.values.variables[10] = 1;
    assert(force_feedback_script_tick(&expired, 1, 0, 1000).output_policy ==
           FORCE_FEEDBACK_SCRIPT_OUTPUT_ZERO);
    assert(force_feedback_script_tick(&expired, 1, 0, 1000).immediate_zero);
    assert(expired.values.slots[0].execution_count == 0);

    ForceFeedbackScriptSystem suppressed = prepare_system(script, sizeof(script));
    suppressed.mode = FORCE_FEEDBACK_RUNTIME_ZERO_OUTPUT;
    suppressed.clock.motion_ticks = 10;
    assert(force_feedback_script_tick(&suppressed, 1, 0, 1000).output_policy ==
           FORCE_FEEDBACK_SCRIPT_OUTPUT_ZERO);
}

static void test_handles_position_only_and_idle_selector(void) {
    const uint8_t selector_script[] = {0xa0, 0x10, 7, 0x50};
    ForceFeedbackScriptSystem idle = prepare_system(selector_script, sizeof(selector_script));
    idle.inputs.status = FORCE_FEEDBACK_SCRIPT_INPUT_POSITION;
    idle.clock.motion_ticks = 10;
    idle.values.motion[4] = float_bits(0.25f);
    assert(force_feedback_script_tick(&idle, 1, 500, 1000).output_policy ==
           FORCE_FEEDBACK_SCRIPT_OUTPUT_NONE);
    assert(idle.values.motion[4] == float_bits(0.25f));

    const uint8_t no_op[] = {0x00};
    ForceFeedbackScriptSystem position_only = prepare_system(no_op, sizeof(no_op));
    position_only.mode = FORCE_FEEDBACK_RUNTIME_POSITION_ONLY;
    position_only.clock.motion_ticks = 10;
    assert(force_feedback_script_tick(&position_only, 1, 500, 1000).output_policy ==
           FORCE_FEEDBACK_SCRIPT_OUTPUT_ZERO);
    assert(position_only.values.motion[4] == float_bits(0.5f));
    assert(position_only.values.variables[10] == 0);
    assert(position_only.values.slots[0].execution_count == 0);
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

static void test_updates_report_only_for_selected_output(void) {
    const uint8_t script[] = {0xa0, 0x02, 0x52};
    ForceFeedbackScriptSystem system = prepare_system(script, sizeof(script));
    system.clock.motion_ticks = 10;
    ForceFeedbackScriptOutputState output_state;
    force_feedback_script_output_init(&output_state);
    ForceFeedbackScriptOutputConfig config = full_output_config();
    ForceOutputReport report = {.secondary_magnitude = 1234};

    ForceFeedbackScriptTickResult result =
        force_feedback_script_tick_output(&system, &output_state, 1, 0, 1000, &config, &report);
    assert(result.wrote_output);
    assert(!result.outside_travel);
    assert(report.primary_magnitude == 0);

    report.primary_magnitude = 4321;
    result =
        force_feedback_script_tick_output(&system, &output_state, 2, 0, 1000, &config, &report);
    assert(!result.wrote_output);
    assert(report.primary_magnitude == 4321);

    system.clock.motion_ticks = 20;
    result =
        force_feedback_script_tick_output(&system, &output_state, 7, 0, 1000, &config, &report);
    assert(result.wrote_output);
    assert(report.primary_magnitude == UINT16_MAX);
}

static void test_prioritizes_live_position_and_immediately_clears_expired_force(void) {
    const uint8_t script[] = {0x00};
    ForceFeedbackScriptSystem position = prepare_system(script, sizeof(script));
    position.store.position_request_pending = true;
    ForceFeedbackScriptOutputState output_state;
    force_feedback_script_output_init(&output_state);
    ForceFeedbackScriptOutputConfig config = full_output_config();
    ForceOutputReport report = {0};

    ForceFeedbackScriptTickResult result =
        force_feedback_script_tick_output(&position, &output_state, 1, 500, 1000, &config, &report);
    assert(result.wrote_output);
    assert(!report.positive_direction && report.primary_magnitude > 0);

    ForceFeedbackScriptSystem expired = prepare_system(script, sizeof(script));
    expired.inputs.deadline = 1;
    expired.values.variables[10] = 1;
    report = (ForceOutputReport){
        .positive_direction = true,
        .primary_magnitude = 4321,
        .secondary_magnitude = 1234,
    };
    result =
        force_feedback_script_tick_output(&expired, &output_state, 1, 0, 1000, &config, &report);
    assert(result.wrote_output);
    assert(report.positive_direction && report.primary_magnitude == 0);
    assert(report.secondary_magnitude == 1234);
}

static void test_propagates_slot_faults(void) {
    const uint8_t invalid_script[] = {0x0a};
    ForceFeedbackScriptSystem system = prepare_system(invalid_script, sizeof(invalid_script));
    ForceFeedbackScriptOutputState output_state;
    force_feedback_script_output_init(&output_state);
    ForceFeedbackScriptOutputConfig config = full_output_config();
    ForceOutputReport report = {0};

    ForceFeedbackScriptTickResult result =
        force_feedback_script_tick_output(&system, &output_state, 1, 0, 1000, &config, &report);
    assert(!result.slot_faulted);
    assert(system.values.slots[0].state == FORCE_FEEDBACK_SCRIPT_SLOT_FAULT);

    result =
        force_feedback_script_tick_output(&system, &output_state, 6, 0, 1000, &config, &report);
    assert(!result.slot_faulted);
}

int main(void) {
    test_runs_host_tick_and_exposes_motion();
    test_selects_zero_for_active_expired_and_suppressed_output();
    test_handles_position_only_and_idle_selector();
    test_updates_report_only_for_selected_output();
    test_prioritizes_live_position_and_immediately_clears_expired_force();
    test_propagates_slot_faults();
    return 0;
}
