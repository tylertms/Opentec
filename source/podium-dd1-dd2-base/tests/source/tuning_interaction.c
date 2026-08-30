#include "profile/tuning_interaction.h"

#include <assert.h>
#include <stdbool.h>

static TuningInteractionInput input(uint8_t mode, uint16_t primary, uint16_t secondary) {
    return (TuningInteractionInput){
        .wheel_mode = mode,
        .primary_buttons = primary,
        .secondary_buttons = secondary,
        .available = true,
    };
}

static void open_entries(TuningInteraction *interaction, uint8_t mode) {
    TuningInteractionInput held = input(mode, 0, 0x2000);
    TuningInteractionInput released = input(mode, 0, 0);
    assert(tuning_interaction_update(interaction, &held, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_update(interaction, &released, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(interaction->phase == TUNING_INTERACTION_ENTRY_OPEN);
}

static void test_requests_adjustment_from_profile_hold(void) {
    TuningInteraction interaction;
    TuningInteractionInput held = input(0x10, 0, 0x2040);
    tuning_interaction_init(&interaction);

    assert(tuning_interaction_update(&interaction, &held, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_update(&interaction, &held, 0) ==
           TUNING_INTERACTION_ACTION_PEDAL_ADJUSTMENT);
}

static void test_applies_profile_shortcut_priority(void) {
    static const TuningInteractionInput blocked[] = {
        {.wheel_mode = 0x10, .secondary_buttons = 0x20c0, .available = true},
        {.wheel_mode = 0x0f, .secondary_buttons = 0x2140, .available = true},
        {.wheel_mode = 0x17, .secondary_buttons = 0x2140, .available = true},
        {.wheel_mode = 0x10,
         .secondary_buttons = 0x2040,
         .adapter_profile_shortcut = true,
         .available = true},
    };

    for (unsigned int index = 0; index < sizeof(blocked) / sizeof(blocked[0]); index++) {
        TuningInteraction interaction;
        tuning_interaction_init(&interaction);
        assert(tuning_interaction_update(&interaction, &blocked[index], 0) ==
               TUNING_INTERACTION_ACTION_NONE);
        assert(tuning_interaction_update(&interaction, &blocked[index], 0) ==
               TUNING_INTERACTION_ACTION_NONE);
    }
}

static void test_requests_legacy_adjustment_from_open_entry(void) {
    TuningInteraction interaction;
    tuning_interaction_init(&interaction);
    open_entries(&interaction, 0x0e);

    TuningInteractionInput legacy = input(0x0e, 0, 0x0140);
    assert(tuning_interaction_update(&interaction, &legacy, 0) ==
           TUNING_INTERACTION_ACTION_PEDAL_ADJUSTMENT);
    legacy.primary_buttons = 0x4000;
    legacy.secondary_buttons = 0x0150;
    assert(tuning_interaction_update(&interaction, &legacy, 0) == TUNING_INTERACTION_ACTION_NONE);
    legacy = input(0x10, 0, 0x0140);
    assert(tuning_interaction_update(&interaction, &legacy, 0) == TUNING_INTERACTION_ACTION_NONE);
}

static void test_closes_entries_on_the_next_menu_press(void) {
    TuningInteraction interaction;
    tuning_interaction_init(&interaction);
    open_entries(&interaction, 0x0e);

    TuningInteractionInput held = input(0x0e, 0, 0x2000);
    TuningInteractionInput released = input(0x0e, 0, 0);
    assert(tuning_interaction_update(&interaction, &held, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_update(&interaction, &released, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(interaction.phase == TUNING_INTERACTION_CLOSED);

    TuningInteractionInput legacy = input(0x0e, 0, 0x0140);
    assert(tuning_interaction_update(&interaction, &legacy, 0) == TUNING_INTERACTION_ACTION_NONE);
}

static void test_resets_when_wheel_input_is_unavailable(void) {
    TuningInteraction interaction;
    tuning_interaction_init(&interaction);
    open_entries(&interaction, 0x0e);

    TuningInteractionInput unavailable = {0};
    assert(tuning_interaction_update(&interaction, &unavailable, 0) ==
           TUNING_INTERACTION_ACTION_NONE);
    assert(interaction.phase == TUNING_INTERACTION_CLOSED);
    assert(tuning_interaction_update(NULL, &unavailable, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_update(&interaction, NULL, 0) == TUNING_INTERACTION_ACTION_NONE);
}

static void test_decodes_navigation_priority_and_edges(void) {
    TuningInteraction interaction;
    tuning_interaction_init(&interaction);

    TuningInteractionInput sample = input(0x10, 0x0f00, 0x0200);
    sample.analog_scale = -12;
    TuningNavigationEvent event = tuning_interaction_read_navigation(&interaction, &sample);
    assert(event.mode == TUNING_NAVIGATION_ANALOG);
    assert(event.scale == -12);

    event = tuning_interaction_read_navigation(&interaction, &sample);
    assert(event.mode == TUNING_NAVIGATION_NONE);
    assert(event.scale == 0);

    sample.secondary_buttons |= 0x2000;
    event = tuning_interaction_read_navigation(&interaction, &sample);
    assert(event.mode == TUNING_NAVIGATION_MENU);
    assert(event.scale == 0);
    event = tuning_interaction_read_navigation(&interaction, &sample);
    assert(event.mode == TUNING_NAVIGATION_MENU);
}

static void test_decodes_each_digital_navigation_action(void) {
    static const struct {
        uint16_t primary;
        uint16_t secondary;
        TuningNavigationMode expected;
    } cases[] = {
        {0x0100, 0, TUNING_NAVIGATION_INCREASE},    {0x0800, 0, TUNING_NAVIGATION_DECREASE},
        {0x0200, 0, TUNING_NAVIGATION_PREVIOUS},    {0x0400, 0, TUNING_NAVIGATION_NEXT},
        {0, 0x0200, TUNING_NAVIGATION_TOGGLE_VIEW},
    };

    for (unsigned int index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        TuningInteraction interaction;
        tuning_interaction_init(&interaction);
        TuningInteractionInput sample = input(0x10, cases[index].primary, cases[index].secondary);
        assert(tuning_interaction_read_navigation(&interaction, &sample).mode ==
               cases[index].expected);
        assert(tuning_interaction_read_navigation(&interaction, &sample).mode ==
               TUNING_NAVIGATION_NONE);
        sample.primary_buttons = 0;
        sample.secondary_buttons = 0;
        assert(tuning_interaction_read_navigation(&interaction, &sample).mode ==
               TUNING_NAVIGATION_NONE);
        sample.primary_buttons = cases[index].primary;
        sample.secondary_buttons = cases[index].secondary;
        assert(tuning_interaction_read_navigation(&interaction, &sample).mode ==
               cases[index].expected);
    }
}

static void test_forwards_each_navigation_event_once(void) {
    TuningInteraction interaction;
    tuning_interaction_init(&interaction);
    TuningInteractionInput sample = input(0x10, 0x0400, 0);

    assert(tuning_interaction_update(&interaction, &sample, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_take_navigation(&interaction).mode == TUNING_NAVIGATION_NEXT);
    assert(tuning_interaction_take_navigation(&interaction).mode == TUNING_NAVIGATION_NONE);
    assert(tuning_interaction_take_navigation(NULL).mode == TUNING_NAVIGATION_NONE);
}

static void test_unavailable_navigation_resets_the_edge_latch(void) {
    TuningInteraction interaction;
    tuning_interaction_init(&interaction);
    TuningInteractionInput sample = input(0x10, 0x0100, 0);
    assert(tuning_interaction_read_navigation(&interaction, &sample).mode ==
           TUNING_NAVIGATION_INCREASE);
    sample.available = false;
    assert(tuning_interaction_read_navigation(&interaction, &sample).mode ==
           TUNING_NAVIGATION_NONE);
    sample.available = true;
    assert(tuning_interaction_read_navigation(&interaction, &sample).mode ==
           TUNING_NAVIGATION_INCREASE);
}

static void test_emits_profile_hold_actions_at_their_thresholds(void) {
    TuningInteraction interaction;
    tuning_interaction_init(&interaction);
    TuningInteractionInput held = input(0x10, 0, 0x2000);

    assert(tuning_interaction_update(&interaction, &held, 100) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_update(&interaction, &held, 100) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_update(&interaction, &held, 2099) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_update(&interaction, &held, 2100) ==
           TUNING_INTERACTION_ACTION_TOGGLE_PROFILE_MODE);
    assert(tuning_interaction_update(&interaction, &held, 2101) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_update(&interaction, &held, 10099) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_update(&interaction, &held, 10100) ==
           TUNING_INTERACTION_ACTION_RESET_PROFILES);

    held.secondary_buttons = 0;
    assert(tuning_interaction_update(&interaction, &held, 10101) == TUNING_INTERACTION_ACTION_NONE);
    assert(interaction.phase == TUNING_INTERACTION_CLOSED);
}

static void test_profile_inputs_restart_the_hold_timer(void) {
    TuningInteraction interaction;
    tuning_interaction_init(&interaction);
    TuningInteractionInput held = input(0x10, 0, 0x2000);

    assert(tuning_interaction_update(&interaction, &held, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_update(&interaction, &held, 0) == TUNING_INTERACTION_ACTION_NONE);
    held.primary_buttons = 0x0100;
    assert(tuning_interaction_update(&interaction, &held, 1500) == TUNING_INTERACTION_ACTION_NONE);
    held.primary_buttons = 0;
    assert(tuning_interaction_update(&interaction, &held, 1501) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_update(&interaction, &held, 3500) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_update(&interaction, &held, 3501) ==
           TUNING_INTERACTION_ACTION_TOGGLE_PROFILE_MODE);

    held.profile_selector_active = true;
    assert(tuning_interaction_update(&interaction, &held, 5000) == TUNING_INTERACTION_ACTION_NONE);
    held.profile_selector_active = false;
    assert(tuning_interaction_update(&interaction, &held, 5001) == TUNING_INTERACTION_ACTION_NONE);
}

int main(void) {
    test_requests_adjustment_from_profile_hold();
    test_applies_profile_shortcut_priority();
    test_requests_legacy_adjustment_from_open_entry();
    test_closes_entries_on_the_next_menu_press();
    test_resets_when_wheel_input_is_unavailable();
    test_decodes_navigation_priority_and_edges();
    test_decodes_each_digital_navigation_action();
    test_forwards_each_navigation_event_once();
    test_unavailable_navigation_resets_the_edge_latch();
    test_emits_profile_hold_actions_at_their_thresholds();
    test_profile_inputs_restart_the_hold_timer();
    return 0;
}
