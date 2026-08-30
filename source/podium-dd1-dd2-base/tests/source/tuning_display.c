#include "profile/tuning_display.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "profile/bank.h"
#include "profile/tuning.h"
#include "profile/tuning_entry.h"
#include "profile/tuning_menu.h"
#include "wheel/display_output.h"

static void assert_glyphs(const WheelDisplayOutput *output, uint8_t first, uint8_t second,
                          uint8_t third) {
    assert(output->glyphs[0] == first);
    assert(output->glyphs[1] == second);
    assert(output->glyphs[2] == third);
}

static WheelDisplayOutput render(TuningProfileBank *bank, TuningEntry entry, TuningMenuView view) {
    TuningMenu menu = {.selected_entry = entry, .view = view};
    WheelDisplayOutput output = {.auxiliary = 0xa5, .third_glyph_marker = true};
    assert(tuning_display_render(&menu, bank, &output));
    assert(output.auxiliary == 0xa5);
    assert(!output.third_glyph_marker);
    return output;
}

static void renders_entry_labels_from_the_local_catalog(void) {
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);

    WheelDisplayOutput output = render(&bank, TUNING_ENTRY_NATURAL_DAMPER, TUNING_MENU_VIEW_LABEL);
    assert_glyphs(&output, 0x54, 0x5e, 0x73);

    output = render(&bank, TUNING_ENTRY_THROTTLE_PEDAL_CURVE, TUNING_MENU_VIEW_LABEL);
    assert_glyphs(&output, 0x78, 0x73, 0x39);
}

static void renders_standard_and_advanced_setups(void) {
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);

    WheelDisplayOutput output = render(&bank, TUNING_ENTRY_SETUP, TUNING_MENU_VIEW_VALUE);
    assert_glyphs(&output, 0x77, 0x08, 0x6d);

    bank.selected_slot = 1;
    output = render(&bank, TUNING_ENTRY_SETUP, TUNING_MENU_VIEW_VALUE);
    assert_glyphs(&output, 0x39, 0x08, 0x6d);

    bank.standard_mode_enabled = false;
    output = render(&bank, TUNING_ENTRY_SETUP, TUNING_MENU_VIEW_VALUE);
    assert_glyphs(&output, 0x6d, 0x08, 0x06);

    bank.selected_slot = 5;
    output = render(&bank, TUNING_ENTRY_SETUP, TUNING_MENU_VIEW_VALUE);
    assert_glyphs(&output, 0x6d, 0x08, 0x6d);
}

static void renders_automatic_and_concrete_sensitivity(void) {
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);

    WheelDisplayOutput output = render(&bank, TUNING_ENTRY_SENSITIVITY, TUNING_MENU_VIEW_VALUE);
    assert_glyphs(&output, 0x77, 0x3e, 0x78);

    bank.slots[0].automatic_rotation = 0;
    bank.slots[0].rotation_degrees = 900;
    output = render(&bank, TUNING_ENTRY_SENSITIVITY, TUNING_MENU_VIEW_VALUE);
    assert_glyphs(&output, 0x6f, 0x3f, 0x3f);

    bank.slots[0].rotation_degrees = 1080;
    output = render(&bank, TUNING_ENTRY_SENSITIVITY, TUNING_MENU_VIEW_VALUE);
    assert_glyphs(&output, 0x06, 0x3f, 0xff);
}

static void renders_disabled_scaled_and_limit_values(void) {
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);

    bank.slots[0].force_feedback_strength = 0;
    WheelDisplayOutput output =
        render(&bank, TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH, TUNING_MENU_VIEW_VALUE);
    assert_glyphs(&output, 0x3f, 0x71, 0x71);

    bank.slots[0].force_feedback_strength = 35;
    output = render(&bank, TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH, TUNING_MENU_VIEW_VALUE);
    assert_glyphs(&output, 0x00, 0x4f, 0x6d);

    bank.slots[0].force_effect_strength = 12;
    output = render(&bank, TUNING_ENTRY_FORCE_EFFECT_STRENGTH, TUNING_MENU_VIEW_VALUE);
    assert_glyphs(&output, 0x06, 0x5b, 0x3f);

    bank.slots[0].brake_force = 0;
    output = render(&bank, TUNING_ENTRY_BRAKE_FORCE, TUNING_MENU_VIEW_VALUE);
    assert_glyphs(&output, 0x38, 0xdc, 0x00);

    bank.slots[0].brake_force = 100;
    output = render(&bank, TUNING_ENTRY_BRAKE_FORCE, TUNING_MENU_VIEW_VALUE);
    assert_glyphs(&output, 0x76, 0xb0, 0x00);
}

static void renders_modes_flags_and_curves(void) {
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);

    WheelDisplayOutput output = render(&bank, TUNING_ENTRY_FORCE_SCALE, TUNING_MENU_VIEW_VALUE);
    assert_glyphs(&output, 0x73, 0x79, 0x77);

    bank.slots[0].paddle_mode = TUNING_CLUTCH_HANDBRAKE;
    output = render(&bank, TUNING_ENTRY_PADDLE_MODE, TUNING_MENU_VIEW_VALUE);
    assert_glyphs(&output, 0x39, 0x00, 0x76);

    bank.slots[0].display_rotation_enabled = 0;
    output = render(&bank, TUNING_ENTRY_DISPLAY_ROTATION, TUNING_MENU_VIEW_VALUE);
    assert_glyphs(&output, 0x5c, 0x71, 0x71);

    bank.slots[0].brake_pedal_curve = TUNING_PEDAL_CURVE_PROGRESSIVE;
    output = render(&bank, TUNING_ENTRY_BRAKE_PEDAL_CURVE, TUNING_MENU_VIEW_VALUE);
    assert_glyphs(&output, 0x73, 0x50, 0x5c);
}

static void renders_every_catalog_entry(void) {
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);
    for (TuningEntry entry = 0; entry < TUNING_ENTRY_COUNT; entry++) {
        render(&bank, entry, TUNING_MENU_VIEW_LABEL);
        render(&bank, entry, TUNING_MENU_VIEW_VALUE);
    }
}

static void rejects_a_closed_or_invalid_presentation(void) {
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);
    TuningMenu menu = {.selected_entry = TUNING_ENTRY_COUNT};
    WheelDisplayOutput output = {.glyphs = {1, 2, 3}, .auxiliary = 4};

    assert(!tuning_display_render(&menu, &bank, &output));
    assert_glyphs(&output, 1, 2, 3);
    assert(!tuning_display_render(NULL, &bank, &output));
    assert(!tuning_display_render(&menu, NULL, &output));
    assert(!tuning_display_render(&menu, &bank, NULL));
}

int main(void) {
    renders_entry_labels_from_the_local_catalog();
    renders_standard_and_advanced_setups();
    renders_automatic_and_concrete_sensitivity();
    renders_disabled_scaled_and_limit_values();
    renders_modes_flags_and_curves();
    renders_every_catalog_entry();
    rejects_a_closed_or_invalid_presentation();
    return 0;
}
