#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "force_feedback/script_motion.h"

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void test_samples_wheel_and_derives_motion(void) {
    ForceFeedbackScriptRuntime runtime = {
        .extended_rotation_range = 0,
        .rotation_range_code = 36,
    };
    ForceFeedbackScriptInputs inputs;
    force_feedback_script_inputs_init(&inputs);
    ForceFeedbackScriptMotionState state = {.tick_snapshot = 10};

    force_feedback_script_motion_update(&runtime, &inputs, &state, 20, 500, 1000, true);

    assert(runtime.motion[4] == float_bits(0.5f));
    assert(runtime.motion[5] == float_bits(500.0f));
    assert(runtime.motion[6] == float_bits(500000.0f));
    assert(runtime.motion[7] == UINT32_C(0x3fc90fdb));
    assert(runtime.axes[0] == runtime.motion[4]);
    assert(runtime.axes[1] == runtime.motion[7]);
    assert(runtime.axes[2] == runtime.motion[5]);
    assert(runtime.axes[3] == runtime.motion[6]);

    force_feedback_script_motion_update(&runtime, &inputs, &state, 30, 250, 1000, true);
    assert(runtime.motion[4] == float_bits(0.25f));
    assert(runtime.motion[5] == float_bits(-250.0f));
    assert(runtime.motion[6] == float_bits(-750000.0f));
}

static void test_scales_angle_from_raw_sensitivity_code(void) {
    const uint8_t raw_sensitivity_code = (uint8_t)((int16_t)36 - 127);
    ForceFeedbackScriptRuntime runtime = {
        .extended_rotation_range = 999,
        .rotation_range_code = raw_sensitivity_code,
    };
    ForceFeedbackScriptInputs inputs;
    force_feedback_script_inputs_init(&inputs);
    ForceFeedbackScriptMotionState state = {.tick_snapshot = 10};

    force_feedback_script_motion_update(&runtime, &inputs, &state, 20, 500, 1000, true);

    assert(runtime.motion[7] == UINT32_C(0xc07e1eb5));
    assert(runtime.motion[7] != UINT32_C(0x3fc90fdb));
}

static void test_integrates_matching_input_and_clamps(void) {
    ForceFeedbackScriptRuntime runtime = {.rotation_range_code = 36};
    runtime.motion[0] = 7;
    runtime.motion[4] = float_bits(0.9f);
    ForceFeedbackScriptInputs inputs;
    force_feedback_script_inputs_init(&inputs);
    inputs.slots[1].status = 7;
    inputs.slots[1].duration = float_bits(0.25f);
    ForceFeedbackScriptMotionState state = {0};

    force_feedback_script_motion_update(&runtime, &inputs, &state, 10, -500, 1000, true);

    assert(runtime.motion[4] == float_bits(1.0f));
    assert(state.previous_position == 1.0f);
}

static void test_retains_position_for_unmatched_nonzero_selector(void) {
    ForceFeedbackScriptRuntime runtime = {.rotation_range_code = 36};
    runtime.motion[0] = 9;
    runtime.motion[4] = float_bits(-0.4f);
    ForceFeedbackScriptInputs inputs;
    force_feedback_script_inputs_init(&inputs);
    ForceFeedbackScriptMotionState state = {0};

    force_feedback_script_motion_update(&runtime, &inputs, &state, 10, 500, 1000, true);

    assert(runtime.motion[4] == float_bits(-0.4f));
}

static void test_can_bypass_matching_live_input(void) {
    ForceFeedbackScriptRuntime runtime = {.rotation_range_code = 36};
    runtime.motion[0] = 7;
    runtime.motion[4] = float_bits(0.5f);
    ForceFeedbackScriptInputs inputs;
    force_feedback_script_inputs_init(&inputs);
    inputs.slots[0].status = 7;
    inputs.slots[0].duration = float_bits(0.25f);
    ForceFeedbackScriptMotionState state = {0};

    force_feedback_script_motion_update(&runtime, &inputs, &state, 10, -500, 1000, false);

    assert(runtime.motion[4] == float_bits(0.5f));
}

int main(void) {
    test_samples_wheel_and_derives_motion();
    test_scales_angle_from_raw_sensitivity_code();
    test_integrates_matching_input_and_clamps();
    test_retains_position_for_unmatched_nonzero_selector();
    test_can_bypass_matching_live_input();
    return 0;
}
