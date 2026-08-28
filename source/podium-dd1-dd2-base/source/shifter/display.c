#include "shifter/display.h"

#include <stdbool.h>
#include <stdint.h>

#include "shifter/h_pattern.h"
#include "wheel/display_output.h"

enum {
    GEAR_DISPLAY_DURATION_MS = 1000,
    GEAR_DISPLAY_POSITION = 1,
    GLYPH_REVERSE = 0x50,
};

static uint8_t gear_glyph(ShifterGear gear) {
    static const uint8_t digits[] = {0x00, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07};
    switch (gear) {
    case SHIFTER_GEAR_REVERSE:
        return GLYPH_REVERSE;
    case SHIFTER_GEAR_FIRST:
        return digits[1];
    case SHIFTER_GEAR_SECOND:
        return digits[2];
    case SHIFTER_GEAR_THIRD:
        return digits[3];
    case SHIFTER_GEAR_FOURTH:
        return digits[4];
    case SHIFTER_GEAR_FIFTH:
        return digits[5];
    case SHIFTER_GEAR_SIXTH:
        return digits[6];
    case SHIFTER_GEAR_SEVENTH:
        return digits[7];
    default:
        return 0;
    }
}

static bool display_idle(const WheelDisplayOutput *output) {
    return output->glyphs[0] == 0 && output->glyphs[1] == 0 && output->glyphs[2] == 0;
}

static void clear_glyphs(WheelDisplayOutput *output) {
    output->glyphs[0] = 0;
    output->glyphs[1] = 0;
    output->glyphs[2] = 0;
}

void shifter_display_init(ShifterDisplay *display) { *display = (ShifterDisplay){0}; }

/**
 * Shows a changed non-neutral gear in the center display position for one second.
 *
 * @param display Persistent display phase, last gear, and clear deadline.
 * @param gear Current H-pattern gear bit, or neutral.
 * @param wheel_active Whether the attached-wheel display connection is active.
 * @param now_ms Current millisecond counter.
 * @param output Current display output, updated when a gear is shown or cleared.
 * @return True when the display output changed.
 */
bool shifter_display_update(ShifterDisplay *display, ShifterGear gear, bool wheel_active,
                            uint32_t now_ms, WheelDisplayOutput *output) {
    if (display->phase == SHIFTER_DISPLAY_WAITING) {
        if (wheel_active) {
            display->phase = SHIFTER_DISPLAY_MONITORING;
            display->last_gear = gear;
        }
        return false;
    }

    if (display->phase == SHIFTER_DISPLAY_SHOWING) {
        if (now_ms > display->clear_after_ms || gear == SHIFTER_GEAR_NEUTRAL) {
            clear_glyphs(output);
            display->phase = SHIFTER_DISPLAY_MONITORING;
            return true;
        }
        return false;
    }

    if (!wheel_active) {
        display->phase = SHIFTER_DISPLAY_WAITING;
        return false;
    }
    if (gear == display->last_gear) {
        return false;
    }

    display->last_gear = gear;
    uint8_t glyph = gear_glyph(gear);
    if (glyph == 0 || !display_idle(output)) {
        return false;
    }

    output->glyphs[GEAR_DISPLAY_POSITION] = glyph;
    display->clear_after_ms = now_ms + GEAR_DISPLAY_DURATION_MS;
    display->phase = SHIFTER_DISPLAY_SHOWING;
    return true;
}
