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
    assert(!tuning_interaction_requests_pedal_adjustment(interaction, &held));
    assert(!tuning_interaction_requests_pedal_adjustment(interaction, &released));
    assert(interaction->phase == TUNING_INTERACTION_ENTRY_OPEN);
}

static void test_requests_adjustment_from_profile_hold(void) {
    TuningInteraction interaction;
    TuningInteractionInput held = input(0x10, 0, 0x2040);
    tuning_interaction_init(&interaction);

    assert(!tuning_interaction_requests_pedal_adjustment(&interaction, &held));
    assert(tuning_interaction_requests_pedal_adjustment(&interaction, &held));
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
        assert(!tuning_interaction_requests_pedal_adjustment(&interaction, &blocked[index]));
        assert(!tuning_interaction_requests_pedal_adjustment(&interaction, &blocked[index]));
    }
}

static void test_requests_legacy_adjustment_from_open_entry(void) {
    TuningInteraction interaction;
    tuning_interaction_init(&interaction);
    open_entries(&interaction, 0x0e);

    TuningInteractionInput legacy = input(0x0e, 0, 0x0140);
    assert(tuning_interaction_requests_pedal_adjustment(&interaction, &legacy));
    legacy.primary_buttons = 0x4000;
    legacy.secondary_buttons = 0x0150;
    assert(!tuning_interaction_requests_pedal_adjustment(&interaction, &legacy));
    legacy = input(0x10, 0, 0x0140);
    assert(!tuning_interaction_requests_pedal_adjustment(&interaction, &legacy));
}

static void test_closes_entries_on_the_next_menu_press(void) {
    TuningInteraction interaction;
    tuning_interaction_init(&interaction);
    open_entries(&interaction, 0x0e);

    TuningInteractionInput held = input(0x0e, 0, 0x2000);
    TuningInteractionInput released = input(0x0e, 0, 0);
    assert(!tuning_interaction_requests_pedal_adjustment(&interaction, &held));
    assert(!tuning_interaction_requests_pedal_adjustment(&interaction, &released));
    assert(interaction.phase == TUNING_INTERACTION_CLOSED);

    TuningInteractionInput legacy = input(0x0e, 0, 0x0140);
    assert(!tuning_interaction_requests_pedal_adjustment(&interaction, &legacy));
}

static void test_resets_when_wheel_input_is_unavailable(void) {
    TuningInteraction interaction;
    tuning_interaction_init(&interaction);
    open_entries(&interaction, 0x0e);

    TuningInteractionInput unavailable = {0};
    assert(!tuning_interaction_requests_pedal_adjustment(&interaction, &unavailable));
    assert(interaction.phase == TUNING_INTERACTION_CLOSED);
    assert(!tuning_interaction_requests_pedal_adjustment(NULL, &unavailable));
    assert(!tuning_interaction_requests_pedal_adjustment(&interaction, NULL));
}

int main(void) {
    test_requests_adjustment_from_profile_hold();
    test_applies_profile_shortcut_priority();
    test_requests_legacy_adjustment_from_open_entry();
    test_closes_entries_on_the_next_menu_press();
    test_resets_when_wheel_input_is_unavailable();
    return 0;
}
