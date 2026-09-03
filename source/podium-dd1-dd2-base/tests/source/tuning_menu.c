#include "profile/tuning_menu.h"

#include <assert.h>
#include <stddef.h>

#include "profile/bank.h"
#include "profile/tuning_entry.h"
#include "profile/tuning_interaction.h"
#include "wheel/accessory.h"

static const TuningEntryAdjustmentContext adjustment = {
    .multi_position_automatic_available = true,
};

static const TuningEntryAvailabilityContext availability = {
    .wheel_mode = 0x0e,
    .wheel_accessory_kind = WHEEL_ACCESSORY_EXTENDED,
    .pedal_connection = TUNING_PEDALS_TRANSFER,
    .primary_pedal_calibration_active = true,
    .multi_position_supported = true,
    .wheel_axis_report_enabled = true,
    .vibration_mode_compatible = true,
};

static TuningNavigationEvent navigation(TuningNavigationMode mode, int8_t scale) {
    return (TuningNavigationEvent){.mode = mode, .scale = scale};
}

static void selects_the_first_available_entry_when_opened(void) {
    TuningMenu menu;
    TuningProfileBank bank;
    tuning_menu_init(&menu);
    tuning_profile_bank_defaults(&bank);

    TuningMenuUpdate update = tuning_menu_update(&menu, TUNING_INTERACTION_ENTRY_OPEN,
                                                 navigation(TUNING_NAVIGATION_NONE, 0), &bank,
                                                 &availability, &adjustment);
    assert(update.entry_changed);
    assert(!update.value_changed);
    assert(menu.selected_entry == TUNING_ENTRY_SETUP);
    assert(menu.view == TUNING_MENU_VIEW_LABEL);
}

static void navigates_in_display_order_and_skips_unavailable_entries(void) {
    TuningMenu menu = {.selected_entry = TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH};
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);
    bank.standard_mode_enabled = false;

    TuningMenuUpdate update = tuning_menu_update(&menu, TUNING_INTERACTION_ENTRY_OPEN,
                                                 navigation(TUNING_NAVIGATION_NEXT, 0), &bank,
                                                 &availability, &adjustment);
    assert(update.entry_changed);
    assert(menu.selected_entry == TUNING_ENTRY_VIBRATION_STRENGTH);
    assert(menu.view == TUNING_MENU_VIEW_LABEL);
}

static void adjusts_the_selected_entry(void) {
    TuningMenu menu = {
        .selected_entry = TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH,
        .view = TUNING_MENU_VIEW_LABEL,
    };
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);

    TuningMenuUpdate update = tuning_menu_update(&menu, TUNING_INTERACTION_ENTRY_OPEN,
                                                 navigation(TUNING_NAVIGATION_DECREASE, 0), &bank,
                                                 &availability, &adjustment);
    assert(!update.value_changed);
    assert(!update.adjustment_requested);
    assert(bank.slots[0].force_feedback_strength == 35);
    assert(menu.view == TUNING_MENU_VIEW_VALUE);

    update = tuning_menu_update(&menu, TUNING_INTERACTION_ENTRY_OPEN,
                                navigation(TUNING_NAVIGATION_DECREASE, 0), &bank, &availability,
                                &adjustment);
    assert(update.value_changed);
    assert(update.adjustment_requested);
    assert(update.adjusted_entry == TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH);
    assert(bank.slots[0].force_feedback_strength == 34);
    assert(menu.view == TUNING_MENU_VIEW_VALUE);
}

static void toggles_between_entry_label_and_value(void) {
    TuningMenu menu = {
        .selected_entry = TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH,
        .view = TUNING_MENU_VIEW_LABEL,
    };
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);

    tuning_menu_update(&menu, TUNING_INTERACTION_ENTRY_OPEN,
                       navigation(TUNING_NAVIGATION_TOGGLE_VIEW, 0), &bank, &availability,
                       &adjustment);
    assert(menu.view == TUNING_MENU_VIEW_VALUE);

    tuning_menu_update(&menu, TUNING_INTERACTION_ENTRY_OPEN,
                       navigation(TUNING_NAVIGATION_TOGGLE_VIEW, 0), &bank, &availability,
                       &adjustment);
    assert(menu.view == TUNING_MENU_VIEW_LABEL);
}

static void toggles_setup_between_label_and_value(void) {
    TuningMenu menu = {
        .selected_entry = TUNING_ENTRY_SETUP,
        .view = TUNING_MENU_VIEW_LABEL,
    };
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);

    tuning_menu_update(&menu, TUNING_INTERACTION_ENTRY_OPEN,
                       navigation(TUNING_NAVIGATION_TOGGLE_VIEW, 0), &bank, &availability,
                       &adjustment);
    assert(menu.view == TUNING_MENU_VIEW_VALUE);

    tuning_menu_update(&menu, TUNING_INTERACTION_ENTRY_OPEN,
                       navigation(TUNING_NAVIGATION_TOGGLE_VIEW, 0), &bank, &availability,
                       &adjustment);
    assert(menu.view == TUNING_MENU_VIEW_LABEL);
}

static void navigates_from_an_unavailable_raw_entry(void) {
    TuningMenu menu = {
        .selected_entry = TUNING_ENTRY_FULL_FORCE,
        .view = TUNING_MENU_VIEW_VALUE,
    };
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);

    TuningMenuUpdate update = tuning_menu_update(&menu, TUNING_INTERACTION_ENTRY_OPEN,
                                                 navigation(TUNING_NAVIGATION_NEXT, 0), &bank,
                                                 &availability, &adjustment);
    assert(update.entry_changed);
    assert(menu.selected_entry == TUNING_ENTRY_BUTTON_ILLUMINATION);
    assert(menu.view == TUNING_MENU_VIEW_LABEL);
}

static void resets_selection_for_the_next_menu_open(void) {
    TuningMenu menu = {
        .selected_entry = TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH,
        .view = TUNING_MENU_VIEW_VALUE,
    };
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);

    tuning_menu_reset(&menu);
    assert(menu.selected_entry == TUNING_ENTRY_COUNT);
    assert(menu.view == TUNING_MENU_VIEW_LABEL);

    TuningMenuUpdate update = tuning_menu_update(&menu, TUNING_INTERACTION_ENTRY_OPEN,
                                                 navigation(TUNING_NAVIGATION_NONE, 0), &bank,
                                                 &availability, &adjustment);
    assert(update.entry_changed);
    assert(menu.selected_entry == TUNING_ENTRY_SETUP);
    assert(menu.view == TUNING_MENU_VIEW_LABEL);
}

static void repairs_an_unavailable_selection_and_clears_it_when_closed(void) {
    TuningMenu menu = {.selected_entry = TUNING_ENTRY_FULL_FORCE};
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);

    TuningMenuUpdate update = tuning_menu_update(&menu, TUNING_INTERACTION_ENTRY_OPEN,
                                                 navigation(TUNING_NAVIGATION_NONE, 0), &bank,
                                                 &availability, &adjustment);
    assert(update.entry_changed);
    assert(menu.selected_entry == TUNING_ENTRY_SETUP);

    update =
        tuning_menu_update(&menu, TUNING_INTERACTION_CLOSED, navigation(TUNING_NAVIGATION_NONE, 0),
                           &bank, &availability, &adjustment);
    assert(update.entry_changed);
    assert(menu.selected_entry == TUNING_ENTRY_COUNT);
    assert(menu.view == TUNING_MENU_VIEW_LABEL);
}

static void handles_unavailable_state(void) {
    TuningMenu menu;
    TuningProfileBank bank;
    tuning_menu_init(&menu);
    tuning_profile_bank_defaults(&bank);

    assert(!tuning_menu_update(NULL, TUNING_INTERACTION_ENTRY_OPEN,
                               navigation(TUNING_NAVIGATION_NONE, 0), &bank, &availability,
                               &adjustment)
                .entry_changed);
    assert(!tuning_menu_update(&menu, TUNING_INTERACTION_ENTRY_OPEN,
                               navigation(TUNING_NAVIGATION_NONE, 0), NULL, &availability,
                               &adjustment)
                .entry_changed);
}

int main(void) {
    selects_the_first_available_entry_when_opened();
    navigates_in_display_order_and_skips_unavailable_entries();
    adjusts_the_selected_entry();
    toggles_between_entry_label_and_value();
    toggles_setup_between_label_and_value();
    navigates_from_an_unavailable_raw_entry();
    resets_selection_for_the_next_menu_open();
    repairs_an_unavailable_selection_and_clears_it_when_closed();
    handles_unavailable_state();
    return 0;
}
