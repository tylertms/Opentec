#include "profile/tuning_interaction.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

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

static void test_profile_shortcuts(void) {
    TuningInteraction interaction;
    tuning_interaction_init(&interaction);
    TuningInteractionInput held = input(0x10, 0, 0x2040);
    assert(tuning_interaction_update(&interaction, &held, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_update(&interaction, &held, 0) ==
           TUNING_INTERACTION_ACTION_PEDAL_ADJUSTMENT);

    tuning_interaction_init(&interaction);
    held = input(0x0f, 0, 0x2100);
    assert(tuning_interaction_update(&interaction, &held, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_update(&interaction, &held, 0) ==
           TUNING_INTERACTION_ACTION_SHOW_SHIFTER);

    tuning_interaction_init(&interaction);
    held = input(0x1c, 0, 0x2080);
    assert(tuning_interaction_update(&interaction, &held, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_update(&interaction, &held, 0) ==
           TUNING_INTERACTION_ACTION_SHOW_EXTENDED_SHIFTER);

    tuning_interaction_init(&interaction);
    held = input(0x1c, 0, 0x2000);
    held.adapter_connected = true;
    held.adapter_mode = 1;
    held.adapter_buttons[1] = 1;
    assert(tuning_interaction_update(&interaction, &held, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_update(&interaction, &held, 0) ==
           TUNING_INTERACTION_ACTION_SHOW_SHIFTER);
}

static void test_legacy_entry_shortcuts(void) {
    TuningInteraction interaction;
    tuning_interaction_init(&interaction);
    open_entries(&interaction, 0x0e);

    TuningInteractionInput legacy = input(0x0e, 0, 0x0110);
    assert(tuning_interaction_update(&interaction, &legacy, 0) ==
           TUNING_INTERACTION_ACTION_SHOW_SHIFTER);
    legacy.secondary_buttons = 0;
    (void)tuning_interaction_update(&interaction, &legacy, 0);
    legacy.secondary_buttons = 0x0140;
    assert(tuning_interaction_update(&interaction, &legacy, 0) ==
           TUNING_INTERACTION_ACTION_PEDAL_ADJUSTMENT);
    legacy.secondary_buttons = 0;
    (void)tuning_interaction_update(&interaction, &legacy, 0);
    legacy.secondary_buttons = 0x0150;
    assert(tuning_interaction_update(&interaction, &legacy, 0) ==
           (TUNING_INTERACTION_ACTION_SHOW_SHIFTER | TUNING_INTERACTION_ACTION_PEDAL_ADJUSTMENT));
}

static void test_center_capture_chords(void) {
    TuningInteraction interaction;
    tuning_interaction_init(&interaction);
    open_entries(&interaction, 0x0e);
    TuningInteractionInput legacy = input(0x0e, 0x4000, 0x0010);
    assert(tuning_interaction_update(&interaction, &legacy, 0) ==
           TUNING_INTERACTION_ACTION_SHOW_CENTER_CAPTURE);
    assert(interaction.phase == TUNING_INTERACTION_CENTER_CAPTURE);
    legacy.primary_buttons = 0;
    legacy.secondary_buttons = 0;
    assert(tuning_interaction_update(&interaction, &legacy, 0) ==
           TUNING_INTERACTION_ACTION_CAPTURE_CENTER);

    tuning_interaction_init(&interaction);
    open_entries(&interaction, 0x10);
    TuningInteractionInput standard = input(0x10, 0x1000, 0x0080);
    assert(tuning_interaction_update(&interaction, &standard, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(interaction.phase == TUNING_INTERACTION_CENTER_CAPTURE);
    standard.primary_buttons = 0;
    standard.secondary_buttons = 0;
    assert(tuning_interaction_update(&interaction, &standard, 0) ==
           TUNING_INTERACTION_ACTION_CAPTURE_CENTER);

    tuning_interaction_init(&interaction);
    open_entries(&interaction, 0x1c);
    TuningInteractionInput extended = input(0x1c, 0, 0x0200);
    extended.auxiliary_report[2] = 2;
    assert(tuning_interaction_update(&interaction, &extended, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(interaction.phase == TUNING_INTERACTION_CENTER_CAPTURE);
}

static void test_adapter_center_chords(void) {
    TuningInteraction interaction;
    tuning_interaction_init(&interaction);
    open_entries(&interaction, 0x10);
    TuningInteractionInput adapter = input(0x10, 0, 0);
    adapter.adapter_connected = true;
    adapter.adapter_mode = 1;
    adapter.adapter_buttons[0] = 0x10;
    adapter.adapter_buttons[1] = 0x04;
    assert(tuning_interaction_update(&interaction, &adapter, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(interaction.phase == TUNING_INTERACTION_CENTER_CAPTURE);

    tuning_interaction_init(&interaction);
    open_entries(&interaction, 0x10);
    adapter = input(0x10, 0, 0);
    adapter.adapter_connected = true;
    adapter.adapter_buttons[2] = 0x0c;
    assert(tuning_interaction_update(&interaction, &adapter, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(interaction.phase == TUNING_INTERACTION_CENTER_CAPTURE);
}

static void test_v3_pedal_operations(void) {
    static const struct {
        uint16_t primary;
        TuningInteractionPhase phase;
        TuningInteractionAction start;
        TuningInteractionAction complete;
    } cases[] = {
        {0x0100, TUNING_INTERACTION_PEDAL_UP, TUNING_INTERACTION_ACTION_PEDAL_UP,
         TUNING_INTERACTION_ACTION_PEDAL_UP_COMPLETE},
        {0x0800, TUNING_INTERACTION_PEDAL_DOWN, TUNING_INTERACTION_ACTION_PEDAL_DOWN,
         TUNING_INTERACTION_ACTION_PEDAL_DOWN_COMPLETE},
        {0x0200, TUNING_INTERACTION_PEDAL_AUTOMATIC, TUNING_INTERACTION_ACTION_PEDAL_AUTOMATIC,
         TUNING_INTERACTION_ACTION_PEDAL_AUTOMATIC_COMPLETE},
    };
    for (unsigned int index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        TuningInteraction interaction;
        tuning_interaction_init(&interaction);
        open_entries(&interaction, 0x0e);
        TuningInteractionInput sample = input(0x0e, cases[index].primary, 0x0009);
        sample.entry_showing_label = true;
        sample.legacy_pedal_calibration_available = true;
        assert(tuning_interaction_update(&interaction, &sample, 10) ==
               TUNING_INTERACTION_ACTION_NONE);
        assert(interaction.phase == cases[index].phase);
        sample.primary_buttons = 0;
        sample.secondary_buttons = 0;
        assert(tuning_interaction_update(&interaction, &sample, 11) == cases[index].start);
        sample.pedal_operation_pending = true;
        assert(tuning_interaction_update(&interaction, &sample, 12) ==
               TUNING_INTERACTION_ACTION_NONE);
        sample.pedal_operation_pending = false;
        assert(tuning_interaction_update(&interaction, &sample, 13) == cases[index].complete);
        assert(tuning_interaction_update(&interaction, &sample, 2012) ==
               TUNING_INTERACTION_ACTION_NONE);
        assert(interaction.phase == cases[index].phase);
        assert(tuning_interaction_update(&interaction, &sample, 2013) ==
               TUNING_INTERACTION_ACTION_NONE);
        assert(interaction.phase == TUNING_INTERACTION_ENTRY_OPEN);
    }
}

static void test_v3_pedal_gates(void) {
    TuningInteraction interaction;
    tuning_interaction_init(&interaction);
    open_entries(&interaction, 0x0e);
    TuningInteractionInput sample = input(0x0e, 0x0100, 0x0009);
    assert(tuning_interaction_update(&interaction, &sample, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(interaction.phase == TUNING_INTERACTION_ENTRY_OPEN);
    sample.entry_showing_label = true;
    sample.legacy_pedal_calibration_available = true;
    sample.secondary_buttons = 0x0008;
    assert(tuning_interaction_update(&interaction, &sample, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(interaction.phase == TUNING_INTERACTION_ENTRY_OPEN);
}

static void test_host_and_adapter_suppression_predicates(void) {
    TuningInteraction interaction = {0};
    assert(!tuning_interaction_suppresses_host_input(&interaction));
    assert(!tuning_interaction_suppresses_system_button(&interaction));
    assert(!tuning_interaction_blocks_adapter_synchronization(&interaction));

    interaction.phase = TUNING_INTERACTION_ENTRY_OPEN;
    assert(tuning_interaction_suppresses_host_input(&interaction));
    assert(tuning_interaction_suppresses_system_button(&interaction));
    assert(tuning_interaction_blocks_adapter_synchronization(&interaction));
    interaction.phase = TUNING_INTERACTION_CENTER_CAPTURE;
    assert(tuning_interaction_suppresses_host_input(&interaction));
    assert(tuning_interaction_suppresses_system_button(&interaction));
    assert(!tuning_interaction_blocks_adapter_synchronization(&interaction));
    interaction.phase = TUNING_INTERACTION_MENU_HELD;
    assert(tuning_interaction_suppresses_host_input(&interaction));
    assert(!tuning_interaction_suppresses_system_button(&interaction));
    assert(tuning_interaction_blocks_adapter_synchronization(&interaction));
    interaction.phase = TUNING_INTERACTION_RESET_RESULT;
    assert(!tuning_interaction_blocks_adapter_synchronization(&interaction));
    interaction.phase = TUNING_INTERACTION_CLOSING;
    assert(tuning_interaction_blocks_adapter_synchronization(&interaction));
}

static void test_profile_hold_timing(void) {
    TuningInteraction interaction;
    tuning_interaction_init(&interaction);
    TuningInteractionInput held = input(0x10, 0, 0x2000);
    assert(tuning_interaction_update(&interaction, &held, 100) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_update(&interaction, &held, 100) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_update(&interaction, &held, 2099) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_update(&interaction, &held, 2100) ==
           TUNING_INTERACTION_ACTION_TOGGLE_PROFILE_MODE);
    assert(tuning_interaction_update(&interaction, &held, 10100) ==
           TUNING_INTERACTION_ACTION_RESET_PROFILES);
    assert(interaction.phase == TUNING_INTERACTION_RESET_RESULT);
    held.secondary_buttons = 0;
    assert(tuning_interaction_update(&interaction, &held, 12099) == TUNING_INTERACTION_ACTION_NONE);
    assert(interaction.phase == TUNING_INTERACTION_RESET_RESULT);
    assert(tuning_interaction_update(&interaction, &held, 12100) == TUNING_INTERACTION_ACTION_NONE);
    assert(interaction.phase == TUNING_INTERACTION_CLOSING);
    assert(tuning_interaction_update(&interaction, &held, 12101) == TUNING_INTERACTION_ACTION_NONE);
    assert(interaction.phase == TUNING_INTERACTION_CLOSED);
}

static void test_profile_hold_resets_for_input_activity(void) {
    static const struct {
        uint8_t wheel_mode;
        uint16_t primary_buttons;
        int8_t analog_scale;
        bool profile_selector_active;
        bool resets_hold;
    } cases[] = {
        {0x10, 0x0100, 0, false, true}, {0x10, 0x0200, 0, false, true},
        {0x10, 0x0400, 0, false, true}, {0x10, 0x0800, 0, false, true},
        {0x10, 0, -12, false, true},    {0x10, 0, 0, true, true},
        {0x1c, 0, 0, true, true},       {0x0e, 0, 0, true, false},
        {0x11, 0, 0, true, false},      {0x10, 0x1000, 0, false, false},
    };

    for (unsigned int index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        TuningInteraction interaction;
        tuning_interaction_init(&interaction);
        TuningInteractionInput held = input(cases[index].wheel_mode, 0, 0x2000);
        assert(tuning_interaction_update(&interaction, &held, 100) ==
               TUNING_INTERACTION_ACTION_NONE);
        assert(tuning_interaction_update(&interaction, &held, 100) ==
               TUNING_INTERACTION_ACTION_NONE);
        TuningInteractionInput activity = held;
        activity.primary_buttons = cases[index].primary_buttons;
        activity.analog_scale = cases[index].analog_scale;
        activity.profile_selector_active = cases[index].profile_selector_active;
        assert(tuning_interaction_update(&interaction, &activity, 200) ==
               TUNING_INTERACTION_ACTION_NONE);
        assert(interaction.profile_hold_active != cases[index].resets_hold);
        assert(tuning_interaction_update(&interaction, &held, 201) ==
               TUNING_INTERACTION_ACTION_NONE);
        if (cases[index].resets_hold) {
            assert(tuning_interaction_update(&interaction, &held, 2200) ==
                   TUNING_INTERACTION_ACTION_NONE);
            assert(tuning_interaction_update(&interaction, &held, 2201) ==
                   TUNING_INTERACTION_ACTION_TOGGLE_PROFILE_MODE);
        } else {
            assert(tuning_interaction_update(&interaction, &held, 2100) ==
                   TUNING_INTERACTION_ACTION_TOGGLE_PROFILE_MODE);
        }
    }
}

static void test_extended_reconnect_grace(void) {
    TuningInteraction interaction;
    tuning_interaction_init(&interaction);
    TuningInteractionInput held = input(0x1c, 0, 0x2000);
    assert(tuning_interaction_update(&interaction, &held, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(interaction.phase == TUNING_INTERACTION_MENU_HELD);
    TuningInteractionInput unavailable = input(0x1c, 0, 0);
    unavailable.available = false;
    assert(tuning_interaction_update(&interaction, &unavailable, 100) ==
           TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_update(&interaction, &unavailable, 1599) ==
           TUNING_INTERACTION_ACTION_NONE);
    assert(interaction.phase == TUNING_INTERACTION_MENU_HELD);
    assert(tuning_interaction_update(&interaction, &unavailable, 1600) ==
           TUNING_INTERACTION_ACTION_NONE);
    assert(interaction.phase == TUNING_INTERACTION_CLOSED);
}

static void test_navigation(void) {
    TuningInteraction interaction;
    tuning_interaction_init(&interaction);
    TuningInteractionInput sample = input(0x10, 0x0f00, 0x0200);
    sample.analog_scale = -12;
    TuningNavigationEvent event = tuning_interaction_read_navigation(&interaction, &sample);
    assert(event.mode == TUNING_NAVIGATION_ANALOG);
    assert(event.scale == -12);
    assert(tuning_interaction_read_navigation(&interaction, &sample).mode ==
           TUNING_NAVIGATION_NONE);
    sample.secondary_buttons |= 0x2000;
    assert(tuning_interaction_read_navigation(&interaction, &sample).mode ==
           TUNING_NAVIGATION_MENU);
    assert(tuning_interaction_read_navigation(&interaction, &sample).mode ==
           TUNING_NAVIGATION_MENU);

    tuning_interaction_init(&interaction);
    sample = input(0x10, 0x0400, 0);
    assert(tuning_interaction_update(&interaction, &sample, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_take_navigation(&interaction).mode == TUNING_NAVIGATION_NEXT);
    assert(tuning_interaction_take_navigation(&interaction).mode == TUNING_NAVIGATION_NONE);
    assert(tuning_interaction_take_navigation(NULL).mode == TUNING_NAVIGATION_NONE);
}

static void test_invalid_inputs(void) {
    TuningInteraction interaction;
    tuning_interaction_init(&interaction);
    open_entries(&interaction, 0x0e);
    TuningInteractionInput unavailable = {0};
    assert(tuning_interaction_update(&interaction, &unavailable, 0) ==
           TUNING_INTERACTION_ACTION_NONE);
    assert(interaction.phase == TUNING_INTERACTION_CLOSED);
    assert(tuning_interaction_update(NULL, &unavailable, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(tuning_interaction_update(&interaction, NULL, 0) == TUNING_INTERACTION_ACTION_NONE);
    assert(!tuning_interaction_suppresses_host_input(NULL));
    assert(!tuning_interaction_suppresses_system_button(NULL));
    assert(!tuning_interaction_blocks_adapter_synchronization(NULL));
}

static void test_requests_close(void) {
    TuningInteraction interaction;
    tuning_interaction_init(&interaction);
    interaction.phase = TUNING_INTERACTION_ENTRY_OPEN;
    interaction.navigation = (TuningNavigationEvent){.mode = TUNING_NAVIGATION_NEXT, .scale = 7};

    tuning_interaction_request_close(&interaction);

    assert(interaction.phase == TUNING_INTERACTION_CLOSING);
    assert(interaction.closing);
    assert(interaction.navigation.mode == TUNING_NAVIGATION_NONE);
    assert(interaction.navigation.scale == 0);
    tuning_interaction_request_close(NULL);
}

int main(void) {
    test_profile_shortcuts();
    test_legacy_entry_shortcuts();
    test_center_capture_chords();
    test_adapter_center_chords();
    test_v3_pedal_operations();
    test_v3_pedal_gates();
    test_host_and_adapter_suppression_predicates();
    test_profile_hold_timing();
    test_profile_hold_resets_for_input_activity();
    test_extended_reconnect_grace();
    test_navigation();
    test_invalid_inputs();
    test_requests_close();
    return 0;
}
