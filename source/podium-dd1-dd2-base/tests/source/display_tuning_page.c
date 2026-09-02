#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "display/framebuffer.h"
#include "display/text.h"
#include "display/tuning_page.h"
#include "profile/bank.h"
#include "profile/tuning.h"
#include "profile/tuning_entry.h"
#include "profile/tuning_menu.h"

enum { DISPLAY_ROW_BYTES = DISPLAY_FRAMEBUFFER_WIDTH / 2 };

static uint8_t pixel(const uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], uint16_t x, uint16_t y) {
    uint8_t packed = framebuffer[y * DISPLAY_ROW_BYTES + x / 2];
    return (x & 1u) != 0 ? packed & 0x0fu : packed >> 4;
}

static bool has_lit_pixel(const uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], uint16_t first_y,
                          uint16_t last_y) {
    for (uint16_t y = first_y; y <= last_y; y++) {
        for (uint16_t x = 0; x < DISPLAY_FRAMEBUFFER_WIDTH; x++) {
            if (pixel(framebuffer, x, y) != 0) {
                return true;
            }
        }
    }
    return false;
}

static TuningPageContent present(TuningProfileBank *bank, TuningEntry entry, TuningMenuView view) {
    TuningMenu menu = {.selected_entry = entry, .view = view};
    TuningPageContent content;
    assert(display_tuning_page_present(&menu, bank, &content));
    return content;
}

static void presents_the_complete_direct_catalog(void) {
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);

    for (TuningEntry entry = TUNING_ENTRY_SETUP; entry < TUNING_ENTRY_COUNT; entry++) {
        TuningPageContent content = present(&bank, entry, TUNING_MENU_VIEW_LABEL);
        assert(content.label[0] != '\0');
        assert(content.title[0] != '\0');
        assert(content.description[0] != '\0');
        assert(content.value[0] != '\0');
    }

    TuningPageContent content = present(&bank, TUNING_ENTRY_FORCE_SCALE, TUNING_MENU_VIEW_LABEL);
    assert(strcmp(content.label, "FFS") == 0);
    assert(strcmp(content.title, "FF Scale") == 0);
    assert(strcmp(content.description, "Force Feedback scaling mode") == 0);

    content = present(&bank, TUNING_ENTRY_THROTTLE_PEDAL_CURVE, TUNING_MENU_VIEW_LABEL);
    assert(strcmp(content.label, "TPC") == 0);
    assert(strcmp(content.title, "Throttle Pedal Character.") == 0);
    assert(strcmp(content.description, "Throttle pedal input curve adjustment") == 0);
}

static void presents_automatic_standard_and_advanced_setups(void) {
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);

    TuningPageContent content = present(&bank, TUNING_ENTRY_SETUP, TUNING_MENU_VIEW_VALUE);
    assert(strcmp(content.title, "Auto Setup") == 0);
    assert(strcmp(content.value, "Auto Setup") == 0);
    assert(strcmp(content.description, "Values set by game or default") == 0);

    bank.selected_slot = 1;
    content = present(&bank, TUNING_ENTRY_SETUP, TUNING_MENU_VIEW_VALUE);
    assert(strcmp(content.title, "Custom Setup") == 0);
    assert(strcmp(content.description, "Values set by user") == 0);

    bank.standard_mode_enabled = false;
    content = present(&bank, TUNING_ENTRY_SETUP, TUNING_MENU_VIEW_VALUE);
    assert(strcmp(content.title, "Setup 1") == 0);

    bank.selected_slot = 5;
    content = present(&bank, TUNING_ENTRY_SETUP, TUNING_MENU_VIEW_VALUE);
    assert(strcmp(content.title, "Setup 5") == 0);
}

static void presents_numeric_limits_modes_and_curves(void) {
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);
    TuningProfile *profile = &bank.slots[bank.selected_slot];

    profile->automatic_rotation = 0;
    profile->rotation_degrees = 1080;
    TuningPageContent content = present(&bank, TUNING_ENTRY_SENSITIVITY, TUNING_MENU_VIEW_VALUE);
    assert(strcmp(content.value, "1080") == 0);

    profile->force_effect_strength = 12;
    content = present(&bank, TUNING_ENTRY_FORCE_EFFECT_STRENGTH, TUNING_MENU_VIEW_VALUE);
    assert(strcmp(content.value, "120") == 0);

    profile->brake_force = 0;
    content = present(&bank, TUNING_ENTRY_BRAKE_FORCE, TUNING_MENU_VIEW_VALUE);
    assert(strcmp(content.value, "MIN.") == 0);
    profile->brake_force = 100;
    content = present(&bank, TUNING_ENTRY_BRAKE_FORCE, TUNING_MENU_VIEW_VALUE);
    assert(strcmp(content.value, "MAX.") == 0);

    profile->multi_position_mode = TUNING_MULTI_POSITION_AUTOMATIC;
    content = present(&bank, TUNING_ENTRY_MULTI_POSITION_MODE, TUNING_MENU_VIEW_VALUE);
    assert(strcmp(content.value, "AUTO") == 0);

    profile->paddle_mode = TUNING_CLUTCH_HANDBRAKE;
    content = present(&bank, TUNING_ENTRY_PADDLE_MODE, TUNING_MENU_VIEW_VALUE);
    assert(strcmp(content.value, "Clutch + Handbrake") == 0);

    profile->brake_pedal_curve = TUNING_PEDAL_CURVE_DEGREES;
    content = present(&bank, TUNING_ENTRY_BRAKE_PEDAL_CURVE, TUNING_MENU_VIEW_VALUE);
    assert(strcmp(content.value, "Degressive ") == 0);
}

static void renders_official_records_and_progress_rows(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);
    TuningMenu menu = {
        .selected_entry = TUNING_ENTRY_BRAKE_INDICATOR_LEVEL,
        .view = TUNING_MENU_VIEW_LABEL,
    };

    assert(display_tuning_page_render(framebuffer, &menu, &bank));
    assert(pixel(framebuffer, 29, 30) == 8);
    assert(pixel(framebuffer, 29, 62) == 8);
    assert(pixel(framebuffer, 29, 19) == 8);
    assert(pixel(framebuffer, 30, 19) == 0);
    assert(pixel(framebuffer, 224, 62) == 8);
    assert(has_lit_pixel(framebuffer, 13, 22));
    assert(has_lit_pixel(framebuffer, 30, 50));
    assert(has_lit_pixel(framebuffer, 48, 57));
    assert(pixel(framebuffer, 30, 47) == 0);

    menu.view = TUNING_MENU_VIEW_VALUE;
    bank.slots[bank.selected_slot].brake_indicator_level = 50;
    assert(display_tuning_page_render(framebuffer, &menu, &bank));
    assert(has_lit_pixel(framebuffer, 30, 50));
    assert(pixel(framebuffer, 30, 47) == 2);
    assert(pixel(framebuffer, 124, 47) == 2);
    assert(pixel(framebuffer, 125, 47) == 0);
}

static void supports_every_character_used_by_tuning_pages(void) {
    static const char characters[] = "%+/ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};

    for (uint8_t index = 0; characters[index] != '\0'; index++) {
        display_framebuffer_clear(framebuffer);
        char text[] = {characters[index], '\0'};
        display_text_draw(framebuffer, text, 0, 0, 1, 15);
        assert(has_lit_pixel(framebuffer, 0, 9));
    }
}

static void rejects_closed_or_incomplete_pages(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    TuningProfileBank bank;
    tuning_profile_bank_defaults(&bank);
    TuningMenu closed = {.selected_entry = TUNING_ENTRY_COUNT};
    TuningPageContent content;

    assert(!display_tuning_page_present(NULL, &bank, &content));
    assert(!display_tuning_page_present(&closed, NULL, &content));
    assert(!display_tuning_page_present(&closed, &bank, NULL));
    assert(!display_tuning_page_present(&closed, &bank, &content));
    assert(!display_tuning_page_render(framebuffer, &closed, &bank));
}

static void renders_pedal_operation_results(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    assert(display_tuning_operation_render(framebuffer, TUNING_INTERACTION_PEDAL_UP));
    assert(has_lit_pixel(framebuffer, 30, 50));
    assert(display_tuning_operation_render(framebuffer, TUNING_INTERACTION_PEDAL_DOWN));
    assert(display_tuning_operation_render(framebuffer, TUNING_INTERACTION_PEDAL_AUTOMATIC));
    assert(!display_tuning_operation_render(framebuffer, TUNING_INTERACTION_ENTRY_OPEN));
}

int main(void) {
    presents_the_complete_direct_catalog();
    presents_automatic_standard_and_advanced_setups();
    presents_numeric_limits_modes_and_curves();
    renders_official_records_and_progress_rows();
    supports_every_character_used_by_tuning_pages();
    rejects_closed_or_incomplete_pages();
    renders_pedal_operation_results();
    return 0;
}
