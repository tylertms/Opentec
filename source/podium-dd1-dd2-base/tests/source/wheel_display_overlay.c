#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "wheel/display_overlay.h"

static void assert_glyphs(const WheelDisplayOverlay *overlay, uint8_t first, uint8_t second,
                          uint8_t third) {
    assert(overlay->output.glyphs[0] == first);
    assert(overlay->output.glyphs[1] == second);
    assert(overlay->output.glyphs[2] == third);
    assert(!overlay->output.third_glyph_marker);
}

static void assert_reset(const WheelDisplayOverlay *overlay) {
    assert_glyphs(overlay, 0, 0, 0);
    assert(overlay->hold_until_ms == 0);
    assert(overlay->deadline_ms == 0);
    assert(overlay->command == 0);
    assert(overlay->remaining_seconds == UINT8_MAX);
    assert(overlay->phase == WHEEL_DISPLAY_OVERLAY_IDLE);
    assert(!overlay->active);
}

static void test_shows_hold_label_then_countdown(void) {
    WheelDisplayOverlay overlay;
    wheel_display_overlay_init(&overlay);
    assert(overlay.remaining_seconds == UINT8_MAX);
    wheel_display_overlay_begin(&overlay, 0x80, 100);

    assert(overlay.active);
    assert(overlay.phase == WHEEL_DISPLAY_OVERLAY_HOLD_LABEL);
    assert_glyphs(&overlay, 0x73, 0x73, 0x39);
    assert(!wheel_display_overlay_update(&overlay, 849));
    assert(wheel_display_overlay_update(&overlay, 850));
    assert_glyphs(&overlay, 0, 0x06, 0x6d);
    assert(!wheel_display_overlay_update(&overlay, 1849));
    assert(wheel_display_overlay_update(&overlay, 1850));
    assert_glyphs(&overlay, 0, 0x06, 0x66);
    assert(wheel_display_overlay_update(&overlay, 15100));
    assert_reset(&overlay);
}

static void test_maps_short_command_labels(void) {
    static const struct {
        uint8_t command;
        uint8_t glyphs[3];
    } cases[] = {
        {0x91, {0x39, 0, 0x78}}, {0x93, {0, 0x78, 0}}, {0x95, {0, 0x39, 0}},
        {0x90, {0, 0, 0}},       {0xa5, {0, 0, 0}},
    };

    WheelDisplayOverlay overlay;
    for (uint8_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        wheel_display_overlay_init(&overlay);
        wheel_display_overlay_begin(&overlay, cases[index].command, 25);
        assert(overlay.active);
        assert(overlay.phase == WHEEL_DISPLAY_OVERLAY_COMMAND);
        assert_glyphs(&overlay, cases[index].glyphs[0], cases[index].glyphs[1],
                      cases[index].glyphs[2]);
        assert(!wheel_display_overlay_update(&overlay, 2024));
        assert(wheel_display_overlay_update(&overlay, 2025));
        assert_reset(&overlay);
    }
}

static void test_replacement_and_clock_wrap_restart_timing(void) {
    WheelDisplayOverlay overlay;
    wheel_display_overlay_init(&overlay);
    wheel_display_overlay_begin(&overlay, 0x80, UINT32_MAX - 500u);
    assert(!wheel_display_overlay_update(&overlay, 248));
    assert(wheel_display_overlay_update(&overlay, 249));
    assert_glyphs(&overlay, 0, 0x06, 0x6d);

    wheel_display_overlay_begin(&overlay, 0x93, 300);
    assert_glyphs(&overlay, 0, 0x78, 0);
    assert(!wheel_display_overlay_update(&overlay, 2299));
    assert(wheel_display_overlay_update(&overlay, 2300));
    assert_reset(&overlay);
}

int main(void) {
    test_shows_hold_label_then_countdown();
    test_maps_short_command_labels();
    test_replacement_and_clock_wrap_restart_timing();
    return 0;
}
