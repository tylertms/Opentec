#include <assert.h>
#include <stdbool.h>

#include "shifter/display.h"
#include "shifter/h_pattern.h"
#include "wheel/display_output.h"

static void test_waits_for_connection_and_next_gear(void) {
    ShifterDisplay display;
    WheelDisplayOutput output = {0};
    shifter_display_init(&display);

    assert(!shifter_display_update(&display, SHIFTER_GEAR_FIRST, false,
                                   H_PATTERN_CALIBRATION_PROMPT_NONE,
                                   H_PATTERN_CALIBRATION_COMPLETE, 0, &output));
    assert(!shifter_display_update(&display, SHIFTER_GEAR_FIRST, true,
                                   H_PATTERN_CALIBRATION_PROMPT_NONE,
                                   H_PATTERN_CALIBRATION_COMPLETE, 10, &output));
    assert(!shifter_display_update(&display, SHIFTER_GEAR_FIRST, true,
                                   H_PATTERN_CALIBRATION_PROMPT_NONE,
                                   H_PATTERN_CALIBRATION_COMPLETE, 20, &output));
    assert(shifter_display_update(&display, SHIFTER_GEAR_SECOND, true,
                                  H_PATTERN_CALIBRATION_PROMPT_NONE, H_PATTERN_CALIBRATION_COMPLETE,
                                  30, &output));
    assert(output.glyphs[0] == 0);
    assert(output.glyphs[1] == 0x5b);
    assert(output.glyphs[2] == 0);
}

static void test_phase_handling_precedes_connection_gate(void) {
    ShifterDisplay display;
    WheelDisplayOutput output = {0};
    shifter_display_init(&display);

    assert(!shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true,
                                   H_PATTERN_CALIBRATION_PROMPT_NONE,
                                   H_PATTERN_CALIBRATION_COMPLETE, 0, &output));
    assert(shifter_display_update(&display, SHIFTER_GEAR_REVERSE, true,
                                  H_PATTERN_CALIBRATION_PROMPT_NONE, H_PATTERN_CALIBRATION_COMPLETE,
                                  1, &output));
    assert(display.phase == SHIFTER_DISPLAY_SHOWING);
    assert(!shifter_display_update(&display, SHIFTER_GEAR_REVERSE, false,
                                   H_PATTERN_CALIBRATION_PROMPT_NONE,
                                   H_PATTERN_CALIBRATION_COMPLETE, 1001, &output));
    assert(output.glyphs[1] == 0x50);
    assert(display.phase == SHIFTER_DISPLAY_SHOWING);
    assert(shifter_display_update(&display, SHIFTER_GEAR_REVERSE, false,
                                  H_PATTERN_CALIBRATION_PROMPT_NONE, H_PATTERN_CALIBRATION_COMPLETE,
                                  1002, &output));
    assert(output.glyphs[0] == 0);
    assert(output.glyphs[1] == 0);
    assert(output.glyphs[2] == 0);
    assert(display.phase == SHIFTER_DISPLAY_MONITORING);
    assert(!shifter_display_update(&display, SHIFTER_GEAR_REVERSE, false,
                                   H_PATTERN_CALIBRATION_PROMPT_NONE,
                                   H_PATTERN_CALIBRATION_COMPLETE, 1003, &output));
    assert(display.phase == SHIFTER_DISPLAY_WAITING);
}

static void test_monitor_sample_precedes_connection_gate(void) {
    ShifterDisplay display;
    WheelDisplayOutput output = {0};
    shifter_display_init(&display);

    assert(!shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true,
                                   H_PATTERN_CALIBRATION_PROMPT_NONE,
                                   H_PATTERN_CALIBRATION_COMPLETE, 0, &output));
    assert(shifter_display_update(&display, SHIFTER_GEAR_REVERSE, false,
                                  H_PATTERN_CALIBRATION_PROMPT_NONE, H_PATTERN_CALIBRATION_COMPLETE,
                                  1, &output));
    assert(output.glyphs[1] == 0x50);
    assert(display.phase == SHIFTER_DISPLAY_WAITING);
}

static void test_clears_after_strict_one_second_deadline(void) {
    ShifterDisplay display;
    WheelDisplayOutput output = {0};
    shifter_display_init(&display);
    shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true, H_PATTERN_CALIBRATION_PROMPT_NONE,
                           H_PATTERN_CALIBRATION_COMPLETE, 10, &output);
    assert(shifter_display_update(&display, SHIFTER_GEAR_REVERSE, true,
                                  H_PATTERN_CALIBRATION_PROMPT_NONE, H_PATTERN_CALIBRATION_COMPLETE,
                                  100, &output));
    assert(output.glyphs[1] == 0x50);
    assert(!shifter_display_update(&display, SHIFTER_GEAR_REVERSE, true,
                                   H_PATTERN_CALIBRATION_PROMPT_NONE,
                                   H_PATTERN_CALIBRATION_COMPLETE, 1100, &output));
    assert(shifter_display_update(&display, SHIFTER_GEAR_REVERSE, true,
                                  H_PATTERN_CALIBRATION_PROMPT_NONE, H_PATTERN_CALIBRATION_COMPLETE,
                                  1101, &output));
    assert(output.glyphs[1] == 0);
}

static void test_neutral_clears_immediately(void) {
    ShifterDisplay display;
    WheelDisplayOutput output = {0};
    shifter_display_init(&display);
    shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true, H_PATTERN_CALIBRATION_PROMPT_NONE,
                           H_PATTERN_CALIBRATION_COMPLETE, 0, &output);
    assert(shifter_display_update(&display, SHIFTER_GEAR_SEVENTH, true,
                                  H_PATTERN_CALIBRATION_PROMPT_NONE, H_PATTERN_CALIBRATION_COMPLETE,
                                  1, &output));
    assert(output.glyphs[1] == 0x07);
    assert(shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true,
                                  H_PATTERN_CALIBRATION_PROMPT_NONE, H_PATTERN_CALIBRATION_COMPLETE,
                                  2, &output));
    assert(output.glyphs[1] == 0);
}

static void test_does_not_replace_busy_display(void) {
    ShifterDisplay display;
    WheelDisplayOutput output = {.glyphs = {1, 0, 0}};
    shifter_display_init(&display);
    shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true, H_PATTERN_CALIBRATION_PROMPT_NONE,
                           H_PATTERN_CALIBRATION_COMPLETE, 0, &output);
    assert(!shifter_display_update(&display, SHIFTER_GEAR_FIFTH, true,
                                   H_PATTERN_CALIBRATION_PROMPT_NONE,
                                   H_PATTERN_CALIBRATION_COMPLETE, 1, &output));
    assert(output.glyphs[0] == 1);
    assert(output.glyphs[1] == 0);
}

static void test_calibration_stage_and_completion(void) {
    static const uint8_t expected[] = {0x54, 0x50, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07};
    ShifterDisplay display;
    WheelDisplayOutput output = {.glyphs = {1, 2, 3}};
    shifter_display_init(&display);

    assert(!shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true,
                                   H_PATTERN_CALIBRATION_PROMPT_NONE,
                                   H_PATTERN_CALIBRATION_COMPLETE, 0, &output));

    for (HPatternCalibrationPosition position = H_PATTERN_CALIBRATION_NEUTRAL;
         position <= H_PATTERN_CALIBRATION_SEVENTH; position++) {
        assert(shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true,
                                      H_PATTERN_CALIBRATION_PROMPT_POSITION, position,
                                      (uint32_t)position, &output));
        assert(output.glyphs[0] == 0);
        assert(output.glyphs[1] == expected[position]);
        assert(output.glyphs[2] == 0);
    }

    assert(!shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true,
                                   H_PATTERN_CALIBRATION_PROMPT_NONE,
                                   H_PATTERN_CALIBRATION_COMPLETE, 100, &output));
    assert(output.glyphs[1] == 0x07);
    assert(!shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true,
                                   H_PATTERN_CALIBRATION_PROMPT_NONE,
                                   H_PATTERN_CALIBRATION_COMPLETE, 1100, &output));
    assert(shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true,
                                  H_PATTERN_CALIBRATION_PROMPT_NONE, H_PATTERN_CALIBRATION_COMPLETE,
                                  1101, &output));
    assert(output.glyphs[1] == 0);
}

static void test_calibration_entry_prompts(void) {
    ShifterDisplay display;
    WheelDisplayOutput output = {.glyphs = {1, 2, 3}};
    shifter_display_init(&display);

    assert(!shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true,
                                   H_PATTERN_CALIBRATION_PROMPT_WAITING,
                                   H_PATTERN_CALIBRATION_NEUTRAL, 0, &output));
    assert(output.glyphs[0] == 1);
    assert(output.glyphs[1] == 2);
    assert(output.glyphs[2] == 3);

    assert(shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true,
                                  H_PATTERN_CALIBRATION_PROMPT_SHIFTER,
                                  H_PATTERN_CALIBRATION_NEUTRAL, 1, &output));
    assert(output.glyphs[0] == 0x6d);
    assert(output.glyphs[1] == 0x71);
    assert(output.glyphs[2] == 0x78);

    assert(shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true,
                                  H_PATTERN_CALIBRATION_PROMPT_CALIBRATION,
                                  H_PATTERN_CALIBRATION_NEUTRAL, 2, &output));
    assert(output.glyphs[0] == 0x39);
    assert(output.glyphs[1] == 0x77);
    assert(output.glyphs[2] == 0x38);
}

static void test_requested_refresh_shows_current_gear(void) {
    ShifterDisplay display;
    WheelDisplayOutput output = {0};
    shifter_display_init(&display);
    shifter_display_request_refresh(&display);
    assert(!shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, false,
                                   H_PATTERN_CALIBRATION_PROMPT_NONE,
                                   H_PATTERN_CALIBRATION_COMPLETE, 0, &output));
    assert(display.refresh_requested);
    assert(!shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true,
                                   H_PATTERN_CALIBRATION_PROMPT_NONE,
                                   H_PATTERN_CALIBRATION_COMPLETE, 1, &output));
    assert(shifter_display_update(&display, SHIFTER_GEAR_NEUTRAL, true,
                                  H_PATTERN_CALIBRATION_PROMPT_NONE, H_PATTERN_CALIBRATION_COMPLETE,
                                  2, &output));
    assert(output.glyphs[1] == 0x54);
    assert(!display.refresh_requested);
}

static void test_local_display_waits_then_shows_and_clears_gear(void) {
    ShifterDisplay display;
    ShifterLocalDisplay output = {0};
    shifter_display_init(&display);

    assert(!shifter_display_update_local(&display, SHIFTER_GEAR_FIRST, true, true,
                                         H_PATTERN_CALIBRATION_PROMPT_NONE,
                                         H_PATTERN_CALIBRATION_COMPLETE, 0, &output));
    assert(output.kind == SHIFTER_LOCAL_DISPLAY_NONE);
    assert(shifter_display_update_local(&display, SHIFTER_GEAR_SECOND, true, true,
                                        H_PATTERN_CALIBRATION_PROMPT_NONE,
                                        H_PATTERN_CALIBRATION_COMPLETE, 1, &output));
    assert(output.kind == SHIFTER_LOCAL_DISPLAY_GEAR);
    assert(output.glyph == 0x5b);
    assert(!shifter_display_update_local(&display, SHIFTER_GEAR_SECOND, true, true,
                                         H_PATTERN_CALIBRATION_PROMPT_NONE,
                                         H_PATTERN_CALIBRATION_COMPLETE, 1001, &output));
    assert(shifter_display_update_local(&display, SHIFTER_GEAR_SECOND, true, true,
                                        H_PATTERN_CALIBRATION_PROMPT_NONE,
                                        H_PATTERN_CALIBRATION_COMPLETE, 1002, &output));
    assert(output.kind == SHIFTER_LOCAL_DISPLAY_NONE);
}

static void test_local_display_rechecks_gear_after_timeout(void) {
    ShifterDisplay display;
    ShifterLocalDisplay output = {0};
    shifter_display_init(&display);

    shifter_display_update_local(&display, SHIFTER_GEAR_NEUTRAL, true, true,
                                 H_PATTERN_CALIBRATION_PROMPT_NONE, H_PATTERN_CALIBRATION_COMPLETE,
                                 0, &output);
    assert(shifter_display_update_local(&display, SHIFTER_GEAR_FIRST, true, true,
                                        H_PATTERN_CALIBRATION_PROMPT_NONE,
                                        H_PATTERN_CALIBRATION_COMPLETE, 1, &output));
    assert(output.glyph == 0x06);
    assert(shifter_display_update_local(&display, SHIFTER_GEAR_SECOND, true, true,
                                        H_PATTERN_CALIBRATION_PROMPT_NONE,
                                        H_PATTERN_CALIBRATION_COMPLETE, 1002, &output));
    assert(output.kind == SHIFTER_LOCAL_DISPLAY_NONE);
    assert(shifter_display_update_local(&display, SHIFTER_GEAR_SECOND, true, true,
                                        H_PATTERN_CALIBRATION_PROMPT_NONE,
                                        H_PATTERN_CALIBRATION_COMPLETE, 1003, &output));
    assert(output.kind == SHIFTER_LOCAL_DISPLAY_GEAR);
    assert(output.glyph == 0x5b);
}

static void test_local_display_calibration_owns_presentation(void) {
    ShifterDisplay display;
    ShifterLocalDisplay output = {0};
    shifter_display_init(&display);

    assert(shifter_display_update_local(&display, SHIFTER_GEAR_NEUTRAL, true, true,
                                        H_PATTERN_CALIBRATION_PROMPT_WAITING,
                                        H_PATTERN_CALIBRATION_NEUTRAL, 0, &output));
    assert(output.kind == SHIFTER_LOCAL_DISPLAY_CALIBRATION);
    assert(output.calibration_prompt == H_PATTERN_CALIBRATION_PROMPT_WAITING);
    assert(shifter_display_update_local(&display, SHIFTER_GEAR_NEUTRAL, true, true,
                                        H_PATTERN_CALIBRATION_PROMPT_POSITION,
                                        H_PATTERN_CALIBRATION_REVERSE, 1, &output));
    assert(output.calibration_position == H_PATTERN_CALIBRATION_REVERSE);
    assert(shifter_display_update_local(&display, SHIFTER_GEAR_NEUTRAL, true, true,
                                        H_PATTERN_CALIBRATION_PROMPT_NONE,
                                        H_PATTERN_CALIBRATION_COMPLETE, 2, &output));
    assert(output.kind == SHIFTER_LOCAL_DISPLAY_NONE);
}

static void test_local_display_cancels_on_h_pattern_loss(void) {
    ShifterDisplay display;
    ShifterLocalDisplay output = {0};
    shifter_display_init(&display);
    shifter_display_update_local(&display, SHIFTER_GEAR_NEUTRAL, true, true,
                                 H_PATTERN_CALIBRATION_PROMPT_NONE, H_PATTERN_CALIBRATION_COMPLETE,
                                 0, &output);
    shifter_display_update_local(&display, SHIFTER_GEAR_FIRST, true, true,
                                 H_PATTERN_CALIBRATION_PROMPT_NONE, H_PATTERN_CALIBRATION_COMPLETE,
                                 1, &output);
    assert(shifter_display_update_local(&display, SHIFTER_GEAR_FIRST, true, false,
                                        H_PATTERN_CALIBRATION_PROMPT_NONE,
                                        H_PATTERN_CALIBRATION_COMPLETE, 2, &output));
    assert(output.kind == SHIFTER_LOCAL_DISPLAY_NONE);
    assert(display.phase == SHIFTER_DISPLAY_WAITING);
}

static void test_refresh_side_effect_is_separate_from_start_latch(void) {
    ShifterDisplay display;
    shifter_display_init(&display);
    shifter_display_request_refresh(&display);
    assert(display.refresh_requested);
    assert(shifter_display_take_refresh_side_effect(&display));
    assert(!shifter_display_take_refresh_side_effect(&display));
    assert(display.refresh_requested);
}

int main(void) {
    test_waits_for_connection_and_next_gear();
    test_phase_handling_precedes_connection_gate();
    test_monitor_sample_precedes_connection_gate();
    test_clears_after_strict_one_second_deadline();
    test_neutral_clears_immediately();
    test_does_not_replace_busy_display();
    test_calibration_stage_and_completion();
    test_calibration_entry_prompts();
    test_requested_refresh_shows_current_gear();
    test_local_display_waits_then_shows_and_clears_gear();
    test_local_display_rechecks_gear_after_timeout();
    test_local_display_calibration_owns_presentation();
    test_local_display_cancels_on_h_pattern_loss();
    test_refresh_side_effect_is_separate_from_start_latch();
    return 0;
}
