#include "shifter/display.h"

#include <stdbool.h>
#include <stdint.h>

#include "shifter/calibration.h"
#include "shifter/h_pattern.h"
#include "wheel/display_output.h"

enum {
    DISPLAY_HOLD_DURATION_MS = 1000,
    GEAR_DISPLAY_POSITION = 1,
    GLYPH_NEUTRAL = 0x54,
    GLYPH_REVERSE = 0x50,
};

/**
 * @brief Selects the display glyph for an H-pattern gear.
 *
 * Maps reverse to its raw glyph and forward gears one through seven to the corresponding digit.
 * Neutral and unsupported values do not select a glyph.
 *
 * @param[in] gear H-pattern gear to display.
 * @return Seven-segment glyph, or zero when the gear has no display glyph.
 */
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

/**
 * @brief Selects the display glyph for an H-pattern calibration position.
 *
 * Maps neutral and reverse to their raw glyphs and forward gears one through seven to the
 * corresponding digit. Completion does not select a new glyph.
 *
 * @param[in] position Next H-pattern position to capture.
 * @return Seven-segment glyph, or zero after the capture sequence completes.
 */
static uint8_t calibration_glyph(HPatternCalibrationPosition position) {
    switch (position) {
    case H_PATTERN_CALIBRATION_NEUTRAL:
        return GLYPH_NEUTRAL;
    case H_PATTERN_CALIBRATION_REVERSE:
        return GLYPH_REVERSE;
    case H_PATTERN_CALIBRATION_FIRST:
        return gear_glyph(SHIFTER_GEAR_FIRST);
    case H_PATTERN_CALIBRATION_SECOND:
        return gear_glyph(SHIFTER_GEAR_SECOND);
    case H_PATTERN_CALIBRATION_THIRD:
        return gear_glyph(SHIFTER_GEAR_THIRD);
    case H_PATTERN_CALIBRATION_FOURTH:
        return gear_glyph(SHIFTER_GEAR_FOURTH);
    case H_PATTERN_CALIBRATION_FIFTH:
        return gear_glyph(SHIFTER_GEAR_FIFTH);
    case H_PATTERN_CALIBRATION_SIXTH:
        return gear_glyph(SHIFTER_GEAR_SIXTH);
    case H_PATTERN_CALIBRATION_SEVENTH:
        return gear_glyph(SHIFTER_GEAR_SEVENTH);
    case H_PATTERN_CALIBRATION_COMPLETE:
        return 0;
    }
    return 0;
}

/**
 * @brief Determines whether the attached-wheel glyph display is idle.
 *
 * The display is idle only when all three glyph positions are clear.
 *
 * @param[in] output Current attached-wheel display output.
 * @return True when all three glyphs are clear.
 */
static bool display_idle(const WheelDisplayOutput *output) {
    return output->glyphs[0] == 0 && output->glyphs[1] == 0 && output->glyphs[2] == 0;
}

/**
 * @brief Clears the attached-wheel glyph display.
 *
 * Sets all three glyph positions to zero without changing auxiliary display state.
 *
 * @param[out] output Attached-wheel display output to clear.
 */
static void clear_glyphs(WheelDisplayOutput *output) {
    output->glyphs[0] = 0;
    output->glyphs[1] = 0;
    output->glyphs[2] = 0;
}

/**
 * @brief Initializes H-pattern gear display state.
 *
 * Clears the display phase, retained gear, clear deadline, and calibration ownership.
 *
 * @param[out] display Persistent gear display state to initialize.
 */
void shifter_display_init(ShifterDisplay *display) { *display = (ShifterDisplay){0}; }

/**
 * @brief Updates the temporary H-pattern gear display.
 *
 * Calibration owns the center glyph while active and shows the next position to capture. The
 * seventh-gear glyph remains visible for one second after completion. Outside calibration, a
 * changed non-neutral gear is shown for one second when the display is idle. Neutral clears a
 * shown gear, and connection loss returns the service to its waiting phase.
 *
 * @param[in,out] display Persistent display phase, last gear, and clear deadline.
 * @param[in] gear Current H-pattern gear, or neutral.
 * @param[in] wheel_active True while the attached-wheel display connection is active.
 * @param[in] calibration_active True while H-pattern calibration accepts captures.
 * @param[in] calibration_position Next calibration position, or complete.
 * @param[in] now_ms Current millisecond counter.
 * @param[in,out] output Current display output, updated when a gear is shown or cleared.
 * @return True when the display output changed.
 */
bool shifter_display_update(ShifterDisplay *display, ShifterGear gear, bool wheel_active,
                            bool calibration_active,
                            HPatternCalibrationPosition calibration_position, uint32_t now_ms,
                            WheelDisplayOutput *output) {
    if (!wheel_active) {
        display->phase = SHIFTER_DISPLAY_WAITING;
        display->calibration_visible = false;
        return false;
    }

    if (calibration_active) {
        uint8_t glyph = calibration_glyph(calibration_position);
        display->phase = SHIFTER_DISPLAY_MONITORING;
        display->last_gear = gear;
        display->clear_after_ms = 0;
        display->calibration_visible = true;
        if (output->glyphs[0] == 0 && output->glyphs[1] == glyph && output->glyphs[2] == 0) {
            return false;
        }
        clear_glyphs(output);
        output->glyphs[GEAR_DISPLAY_POSITION] = glyph;
        return true;
    }

    if (display->calibration_visible) {
        if (calibration_position == H_PATTERN_CALIBRATION_COMPLETE) {
            if (display->clear_after_ms == 0) {
                display->clear_after_ms = now_ms + DISPLAY_HOLD_DURATION_MS;
                return false;
            }
            if (now_ms <= display->clear_after_ms) {
                return false;
            }
        }
        clear_glyphs(output);
        display->calibration_visible = false;
        display->clear_after_ms = 0;
        display->last_gear = gear;
        return true;
    }

    if (display->phase == SHIFTER_DISPLAY_WAITING) {
        display->phase = SHIFTER_DISPLAY_MONITORING;
        display->last_gear = gear;
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

    if (gear == display->last_gear) {
        return false;
    }

    display->last_gear = gear;
    uint8_t glyph = gear_glyph(gear);
    if (glyph == 0 || !display_idle(output)) {
        return false;
    }

    output->glyphs[GEAR_DISPLAY_POSITION] = glyph;
    display->clear_after_ms = now_ms + DISPLAY_HOLD_DURATION_MS;
    display->phase = SHIFTER_DISPLAY_SHOWING;
    return true;
}
