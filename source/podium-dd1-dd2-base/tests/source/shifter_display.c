#include <assert.h>
#include <stdbool.h>

#include "shifter/display.h"
#include "shifter/h_pattern.h"
#include "wheel/display_output.h"

static void test_waits_for_connection_and_next_gear(void) {
    ShifterDisplay display;
    WheelDisplayOutput output = {0};
    shifter_display_init(&display);

    assert(!shifter_display_update(&display, SHIFTER_GEAR_FIRST, false, 0, &output));
    assert(!shifter_display_update(&display, SHIFTER_GEAR_FIRST, true, 10, &output));
    assert(!shifter_display_update(&display, SHIFTER_GEAR_FIRST, true, 20, &output));
    assert(shifter_display_update(&display, SHIFTER_GEAR_SECOND, true, 30, &output));
    assert(output.glyphs[0] == 0);
    assert(output.glyphs[1] == 0x5b);
    assert(output.glyphs[2] == 0);
}

static void test_clears_after_strict_one_second_deadline(void) {
    ShifterDisplay display;
    WheelDisplayOutput output = {0};
    shifter_display_init(&display);
    shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true, 10, &output);
    assert(shifter_display_update(&display, SHIFTER_GEAR_REVERSE, true, 100, &output));
    assert(output.glyphs[1] == 0x50);
    assert(!shifter_display_update(&display, SHIFTER_GEAR_REVERSE, true, 1100, &output));
    assert(shifter_display_update(&display, SHIFTER_GEAR_REVERSE, true, 1101, &output));
    assert(output.glyphs[1] == 0);
}

static void test_neutral_clears_immediately(void) {
    ShifterDisplay display;
    WheelDisplayOutput output = {0};
    shifter_display_init(&display);
    shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true, 0, &output);
    assert(shifter_display_update(&display, SHIFTER_GEAR_SEVENTH, true, 1, &output));
    assert(output.glyphs[1] == 0x07);
    assert(shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true, 2, &output));
    assert(output.glyphs[1] == 0);
}

static void test_does_not_replace_busy_display(void) {
    ShifterDisplay display;
    WheelDisplayOutput output = {.glyphs = {1, 0, 0}};
    shifter_display_init(&display);
    shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true, 0, &output);
    assert(!shifter_display_update(&display, SHIFTER_GEAR_FIFTH, true, 1, &output));
    assert(output.glyphs[0] == 1);
    assert(output.glyphs[1] == 0);
}

int main(void) {
    test_waits_for_connection_and_next_gear();
    test_clears_after_strict_one_second_deadline();
    test_neutral_clears_immediately();
    test_does_not_replace_busy_display();
    return 0;
}
