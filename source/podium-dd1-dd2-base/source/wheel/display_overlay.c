#include "wheel/display_overlay.h"

#include <stdbool.h>
#include <stdint.h>

#include "platform/time.h"
#include "wheel/display_output.h"

/** @brief Internal display-overlay commands, timings, and glyph patterns. */
enum {
    HOLD_COMMAND = 0x80,                /**< Hold-command code. */
    CLUTCH_AND_THROTTLE_COMMAND = 0x91, /**< Clutch-and-throttle command code. */
    THROTTLE_COMMAND = 0x93,            /**< Throttle command code. */
    CLUTCH_COMMAND = 0x95,              /**< Clutch command code. */
    HOLD_LABEL_DURATION_MS = 750,       /**< Hold-label duration in milliseconds. */
    HOLD_DURATION_MS = 15000,           /**< Hold presentation duration in milliseconds. */
    COMMAND_DURATION_MS = 2000,         /**< Short command presentation duration in milliseconds. */
    COUNTDOWN_ROUNDING_MS = 1749,       /**< Countdown rounding offset in milliseconds. */
    MAXIMUM_COUNTDOWN = 15,             /**< Maximum displayed countdown seconds. */
    GLYPH_C = 0x39,                     /**< Seven-segment C glyph. */
    GLYPH_P = 0x73,                     /**< Seven-segment P glyph. */
    GLYPH_T = 0x78,                     /**< Seven-segment T glyph. */
};

/**
 * @brief Replaces the temporary page's three glyphs.
 *
 * Stores the supplied raw segments and clears the separate third-position marker.
 *
 * @param[out] output Temporary display page to replace.
 * @param[in] first First raw seven-segment glyph.
 * @param[in] second Second raw seven-segment glyph.
 * @param[in] third Third raw seven-segment glyph.
 */
static void set_glyphs(WheelDisplayOutput *output, uint8_t first, uint8_t second, uint8_t third) {
    output->glyphs[0] = first;
    output->glyphs[1] = second;
    output->glyphs[2] = third;
    output->third_glyph_marker = false;
}

/**
 * @brief Encodes one decimal digit as raw seven-segment output.
 *
 * Maps values zero through nine to the segment pattern used by the attached-wheel display.
 *
 * @param[in] digit Decimal digit to encode.
 * @return Raw seven-segment glyph, or zero for a value outside the decimal range.
 */
static uint8_t digit_glyph(uint8_t digit) {
    static const uint8_t glyphs[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
    return digit < sizeof(glyphs) ? glyphs[digit] : 0;
}

/**
 * @brief Renders the temporary command label.
 *
 * Selects the two-pedal, throttle, clutch, or blank triplet associated with the command byte.
 *
 * @param[in,out] overlay Temporary display state receiving the command glyphs.
 */
static void render_command(WheelDisplayOverlay *overlay) {
    uint8_t first = 0;
    uint8_t second = 0;
    uint8_t third = 0;
    if (overlay->command == CLUTCH_AND_THROTTLE_COMMAND) {
        first = GLYPH_C;
        third = GLYPH_T;
    } else if (overlay->command == THROTTLE_COMMAND) {
        second = GLYPH_T;
    } else if (overlay->command == CLUTCH_COMMAND) {
        second = GLYPH_C;
    }
    set_glyphs(&overlay->output, first, second, third);
}

/**
 * @brief Renders the active hold-command countdown.
 *
 * Right-aligns values below ten and uses the final two positions for values ten through fifteen.
 *
 * @param[in,out] overlay Temporary display state receiving the countdown glyphs.
 * @param[in] remaining Remaining whole-second value from one through fifteen.
 */
static void render_countdown(WheelDisplayOverlay *overlay, uint8_t remaining) {
    uint8_t tens = remaining / 10u;
    set_glyphs(&overlay->output, 0, tens == 0 ? 0 : digit_glyph(tens),
               digit_glyph(remaining % 10u));
}

/**
 * @brief Initializes the temporary attached-wheel display page.
 *
 * Clears its output, command, deadlines, phase, and ownership state.
 *
 * @param[out] overlay Temporary display state to initialize.
 */
void wheel_display_overlay_init(WheelDisplayOverlay *overlay) {
    *overlay = (WheelDisplayOverlay){0};
}

/**
 * @brief Starts a temporary attached-wheel command presentation.
 *
 * Command 0x80 shows `PPC` for 750 milliseconds and owns the display for 15 seconds. Other
 * commands show their short label and own the display for two seconds. A new command replaces the
 * current presentation and restarts its timing.
 *
 * @param[out] overlay Temporary display state to start or replace.
 * @param[in] command Command byte selecting the presentation.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void wheel_display_overlay_begin(WheelDisplayOverlay *overlay, uint8_t command, uint32_t now_ms) {
    overlay->command = command;
    overlay->remaining_seconds = UINT8_MAX;
    overlay->active = true;
    if (command == HOLD_COMMAND) {
        overlay->hold_until_ms = now_ms + HOLD_LABEL_DURATION_MS;
        overlay->deadline_ms = now_ms + HOLD_DURATION_MS;
        overlay->phase = WHEEL_DISPLAY_OVERLAY_HOLD_LABEL;
        set_glyphs(&overlay->output, GLYPH_P, GLYPH_P, GLYPH_C);
        return;
    }

    overlay->hold_until_ms = 0;
    overlay->deadline_ms = now_ms + COMMAND_DURATION_MS;
    overlay->phase = WHEEL_DISPLAY_OVERLAY_COMMAND;
    render_command(overlay);
}

/**
 * @brief Advances the temporary attached-wheel command presentation.
 *
 * Retains the initial `PPC` label until its 750-millisecond deadline, then updates a
 * countdown capped at fifteen. At the presentation deadline, clears the temporary page and
 * releases display ownership.
 *
 * @param[in,out] overlay Temporary display state to advance.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when the visible temporary output or ownership state changed.
 */
bool wheel_display_overlay_update(WheelDisplayOverlay *overlay, uint32_t now_ms) {
    if (!overlay->active) {
        return false;
    }
    if (platform_time_reached(now_ms, overlay->deadline_ms)) {
        overlay->output = (WheelDisplayOutput){0};
        overlay->phase = WHEEL_DISPLAY_OVERLAY_IDLE;
        overlay->active = false;
        return true;
    }
    if (overlay->command != HOLD_COMMAND ||
        !platform_time_reached(now_ms, overlay->hold_until_ms)) {
        return false;
    }

    uint32_t rounded_ms = overlay->deadline_ms - now_ms + COUNTDOWN_ROUNDING_MS;
    uint8_t remaining = (uint8_t)(rounded_ms / 1000u);
    if (remaining > MAXIMUM_COUNTDOWN) {
        remaining = MAXIMUM_COUNTDOWN;
    }
    if (overlay->phase == WHEEL_DISPLAY_OVERLAY_COUNTDOWN &&
        overlay->remaining_seconds == remaining) {
        return false;
    }
    render_countdown(overlay, remaining);
    overlay->remaining_seconds = remaining;
    overlay->phase = WHEEL_DISPLAY_OVERLAY_COUNTDOWN;
    return true;
}
