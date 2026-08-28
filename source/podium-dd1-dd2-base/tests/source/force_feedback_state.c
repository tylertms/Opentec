#include <assert.h>
#include <stdint.h>

#include "force_feedback/state.h"

static void test_initializes_effects_and_output_gates(void) {
    ForceFeedbackState state;
    force_feedback_state_init(&state);

    for (uint8_t slot = 0; slot < FORCE_FEEDBACK_EFFECT_SLOT_COUNT; ++slot) {
        assert(state.effects[slot].kind == FORCE_FEEDBACK_EFFECT_NONE);
        assert(!state.effects[slot].active);
    }

    ForceFeedbackEffectState *position = &state.effects[FORCE_FEEDBACK_POSITION_EFFECT_SLOT];
    assert(position->kind == FORCE_FEEDBACK_EFFECT_KIND_2);
    assert(position->active);
    assert(position->kind_2.positions[0] == 0);
    assert(position->kind_2.positions[1] == 0);
    assert(position->kind_2.axis_modes[0] == 4);
    assert(position->kind_2.axis_modes[1] == 4);
    assert(position->kind_2.directions[0] == -1);
    assert(position->kind_2.directions[1] == -1);
    assert(position->kind_2.strength == UINT16_MAX);
    assert(!state.primary_output_disabled);
    assert(!state.secondary_output_disabled);
}

static void test_configures_and_activates_kind_1(void) {
    ForceFeedbackState state;
    force_feedback_state_init(&state);
    ForceFeedbackCommand command = {
        .kind = FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_1,
        .slot = 3,
        .magnitude = -12345,
    };

    assert(force_feedback_state_apply(&state, &command, 90000));
    assert(state.effects[3].kind == FORCE_FEEDBACK_EFFECT_KIND_1);
    assert(state.effects[3].active);
    assert(state.effects[3].kind_1.magnitude == -12345);

    command.kind = FORCE_FEEDBACK_COMMAND_CLEAR_EFFECT;
    assert(force_feedback_state_apply(&state, &command, 90000));
    assert(state.effects[3].kind == FORCE_FEEDBACK_EFFECT_KIND_1);
    assert(!state.effects[3].active);
    assert(state.effects[3].kind_1.magnitude == -12345);
}

static void test_configures_centered_kind_2(void) {
    ForceFeedbackState state;
    force_feedback_state_init(&state);
    ForceFeedbackCommand command = {
        .kind = FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_2,
        .slot = 5,
        .positions = {0, 255},
        .axis_modes = {2, 7},
        .directions = {-1, 1},
        .strength = 0x8080,
    };

    assert(force_feedback_state_apply(&state, &command, 82880));
    ForceFeedbackKind2Effect *effect = &state.effects[5].kind_2;
    assert(state.effects[5].kind == FORCE_FEEDBACK_EFFECT_KIND_2);
    assert(state.effects[5].active);
    assert(effect->positions[0] == -82880);
    assert(effect->positions[1] == 82232);
    assert(effect->axis_modes[0] == 2);
    assert(effect->axis_modes[1] == 7);
    assert(effect->directions[0] == -1);
    assert(effect->directions[1] == 1);
    assert(effect->strength == 0x8080);

    command.positions[0] = 128;
    command.positions[1] = 64;
    assert(force_feedback_state_apply(&state, &command, 6577));
    assert(effect->positions[0] == 0);
    assert(effect->positions[1] == -3289);
}

static void test_configures_kind_3(void) {
    ForceFeedbackState state;
    force_feedback_state_init(&state);
    ForceFeedbackCommand command = {
        .kind = FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_3,
        .slot = 15,
        .axis_modes = {9, 0},
        .directions = {-1, 1},
        .strength = 0x4040,
        .mode = 7,
    };

    assert(force_feedback_state_apply(&state, &command, 0));
    ForceFeedbackKind3Effect *effect = &state.effects[15].kind_3;
    assert(state.effects[15].kind == FORCE_FEEDBACK_EFFECT_KIND_3);
    assert(state.effects[15].active);
    assert(effect->mode == 7);
    assert(effect->axis_mode == 9);
    assert(effect->directions[0] == -1);
    assert(effect->directions[1] == 1);
    assert(effect->strength == 0x4040);
}

static void test_controls_position_effect_and_output_gates(void) {
    ForceFeedbackState state;
    force_feedback_state_init(&state);
    ForceFeedbackCommand command = {
        .kind = FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_1,
        .slot = 0,
        .magnitude = 1,
    };
    assert(force_feedback_state_apply(&state, &command, 0));
    command.slot = 15;
    assert(force_feedback_state_apply(&state, &command, 0));

    command.kind = FORCE_FEEDBACK_COMMAND_CLEAR_POSITION_EFFECT;
    assert(force_feedback_state_apply(&state, &command, 0));
    assert(!state.effects[FORCE_FEEDBACK_POSITION_EFFECT_SLOT].active);
    command.kind = FORCE_FEEDBACK_COMMAND_ACTIVATE_POSITION_EFFECT;
    assert(force_feedback_state_apply(&state, &command, 0));
    assert(state.effects[FORCE_FEEDBACK_POSITION_EFFECT_SLOT].active);

    command.kind = FORCE_FEEDBACK_COMMAND_SET_PRIMARY_OUTPUT;
    command.output_disabled = true;
    assert(force_feedback_state_apply(&state, &command, 0));
    assert(state.primary_output_disabled);
    for (uint8_t slot = 0; slot < FORCE_FEEDBACK_EFFECT_SLOT_COUNT; ++slot) {
        assert(!state.effects[slot].active);
    }
    assert(state.effects[FORCE_FEEDBACK_POSITION_EFFECT_SLOT].active);

    command.kind = FORCE_FEEDBACK_COMMAND_SET_SECONDARY_OUTPUT;
    assert(force_feedback_state_apply(&state, &command, 0));
    assert(state.secondary_output_disabled);
    command.output_disabled = false;
    assert(force_feedback_state_apply(&state, &command, 0));
    assert(!state.secondary_output_disabled);
}

static void test_rejects_invalid_inputs(void) {
    ForceFeedbackState state;
    force_feedback_state_init(&state);
    ForceFeedbackCommand command = {
        .kind = FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_1,
        .slot = FORCE_FEEDBACK_EFFECT_SLOT_COUNT,
    };

    assert(!force_feedback_state_apply(&state, &command, 0));
    command.kind = (ForceFeedbackCommandKind)99;
    command.slot = 0;
    state.effects[0].active = true;
    assert(!force_feedback_state_apply(&state, &command, 0));
    assert(state.effects[0].active);
    assert(!force_feedback_state_apply(0, &command, 0));
    assert(!force_feedback_state_apply(&state, 0, 0));
}

int main(void) {
    test_initializes_effects_and_output_gates();
    test_configures_and_activates_kind_1();
    test_configures_centered_kind_2();
    test_configures_kind_3();
    test_controls_position_effect_and_output_gates();
    test_rejects_invalid_inputs();
    return 0;
}
