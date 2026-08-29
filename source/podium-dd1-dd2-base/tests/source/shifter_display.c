#include <assert.h>
#include <stdbool.h>

#include "shifter/display.h"
#include "shifter/h_pattern.h"
#include "wheel/display_output.h"

static void test_waits_for_connection_and_next_gear(void) {
    ShifterDisplay display;
    WheelDisplayOutput output = {0};
    shifter_display_init(&display);

    assert(!shifter_display_update(&display, SHIFTER_GEAR_FIRST, false, false,
                                   H_PATTERN_CALIBRATION_COMPLETE, 0, &output));
    assert(!shifter_display_update(&display, SHIFTER_GEAR_FIRST, true, false,
                                   H_PATTERN_CALIBRATION_COMPLETE, 10, &output));
    assert(!shifter_display_update(&display, SHIFTER_GEAR_FIRST, true, false,
                                   H_PATTERN_CALIBRATION_COMPLETE, 20, &output));
    assert(shifter_display_update(&display, SHIFTER_GEAR_SECOND, true, false,
                                  H_PATTERN_CALIBRATION_COMPLETE, 30, &output));
    assert(output.glyphs[0] == 0);
    assert(output.glyphs[1] == 0x5b);
    assert(output.glyphs[2] == 0);
}

static void test_clears_after_strict_one_second_deadline(void) {
    ShifterDisplay display;
    WheelDisplayOutput output = {0};
    shifter_display_init(&display);
    shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true, false,
                           H_PATTERN_CALIBRATION_COMPLETE, 10, &output);
    assert(shifter_display_update(&display, SHIFTER_GEAR_REVERSE, true, false,
                                  H_PATTERN_CALIBRATION_COMPLETE, 100, &output));
    assert(output.glyphs[1] == 0x50);
    assert(!shifter_display_update(&display, SHIFTER_GEAR_REVERSE, true, false,
                                   H_PATTERN_CALIBRATION_COMPLETE, 1100, &output));
    assert(shifter_display_update(&display, SHIFTER_GEAR_REVERSE, true, false,
                                  H_PATTERN_CALIBRATION_COMPLETE, 1101, &output));
    assert(output.glyphs[1] == 0);
}

static void test_neutral_clears_immediately(void) {
    ShifterDisplay display;
    WheelDisplayOutput output = {0};
    shifter_display_init(&display);
    shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true, false,
                           H_PATTERN_CALIBRATION_COMPLETE, 0, &output);
    assert(shifter_display_update(&display, SHIFTER_GEAR_SEVENTH, true, false,
                                  H_PATTERN_CALIBRATION_COMPLETE, 1, &output));
    assert(output.glyphs[1] == 0x07);
    assert(shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true, false,
                                  H_PATTERN_CALIBRATION_COMPLETE, 2, &output));
    assert(output.glyphs[1] == 0);
}

static void test_does_not_replace_busy_display(void) {
    ShifterDisplay display;
    WheelDisplayOutput output = {.glyphs = {1, 0, 0}};
    shifter_display_init(&display);
    shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true, false,
                           H_PATTERN_CALIBRATION_COMPLETE, 0, &output);
    assert(!shifter_display_update(&display, SHIFTER_GEAR_FIFTH, true, false,
                                   H_PATTERN_CALIBRATION_COMPLETE, 1, &output));
    assert(output.glyphs[0] == 1);
    assert(output.glyphs[1] == 0);
}

static void test_calibration_stage_and_completion(void) {
    static const uint8_t expected[] = {0x54, 0x50, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07};
    ShifterDisplay display;
    WheelDisplayOutput output = {.glyphs = {1, 2, 3}};
    shifter_display_init(&display);

    for (HPatternCalibrationPosition position = H_PATTERN_CALIBRATION_NEUTRAL;
         position <= H_PATTERN_CALIBRATION_SEVENTH; position++) {
        assert(shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true, true, position,
                                      (uint32_t)position, &output));
        assert(output.glyphs[0] == 0);
        assert(output.glyphs[1] == expected[position]);
        assert(output.glyphs[2] == 0);
    }

    assert(!shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true, false,
                                   H_PATTERN_CALIBRATION_COMPLETE, 100, &output));
    assert(output.glyphs[1] == 0x07);
    assert(!shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true, false,
                                   H_PATTERN_CALIBRATION_COMPLETE, 1100, &output));
    assert(shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true, false,
                                  H_PATTERN_CALIBRATION_COMPLETE, 1101, &output));
    assert(output.glyphs[1] == 0);
}

int main(void) {
    test_waits_for_connection_and_next_gear();
    test_clears_after_strict_one_second_deadline();
    test_neutral_clears_immediately();
    test_does_not_replace_busy_display();
    test_calibration_stage_and_completion();
    return 0;
}
