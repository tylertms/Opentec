#include "shifter/display.h"

#include <stdbool.h>
#include <stdint.h>

#include "shifter/calibration.h"
#include "shifter/h_pattern.h"
#include "wheel/display_output.h"

/**
 * @brief H-pattern display timing, position, and glyph constants.
 */
enum {
    DISPLAY_HOLD_DURATION_MS = 1000,           /**< Duration of a temporary gear glyph. */
    GEAR_DISPLAY_POSITION = 1,                 /**< Display slot used for gear glyphs. */
    CALIBRATION_WAITING_FIRST_POSITION = 0,    /**< Official waiting-prompt first slot. */
    CALIBRATION_WAITING_SECOND_POSITION = 1,   /**< Official waiting-prompt second slot. */
    CALIBRATION_POSITION_DISPLAY_POSITION = 2, /**< Official calibration-position slot. */
    GLYPH_A = 0x77,                            /**< Seven-segment glyph for the letter A. */
    GLYPH_C = 0x39,                            /**< Seven-segment glyph for the letter C. */
    GLYPH_F = 0x71,                            /**< Seven-segment glyph for the letter F. */
    GLYPH_L = 0x38,                            /**< Seven-segment glyph for the letter L. */
    GLYPH_CALIBRATION_WAITING_FIRST = 0x7d,    /**< Official waiting-prompt digit glyph. */
    GLYPH_CALIBRATION_WAITING_SECOND = 0x08,   /**< Official waiting-prompt raw glyph. */
    GLYPH_NEUTRAL = 0x54,                      /**< Seven-segment glyph for neutral. */
    GLYPH_REVERSE = 0x50,                      /**< Seven-segment glyph for reverse. */
    GLYPH_S = 0x6d,                            /**< Seven-segment glyph for the letter S. */
    GLYPH_T = 0x78,                            /**< Seven-segment glyph for the letter T. */
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
    /**
     * @brief Seven-segment glyphs indexed by forward gear number.
     */
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
 * @brief Applies an H-pattern calibration prompt to the attached-wheel glyph page.
 *
 * The official waiting prompt writes its first two slots and preserves the position slot. Each
 * position prompt writes only the third slot, while the shifter and calibration labels replace all
 * three slots. These writes correspond to the 0x03505A-0x035D74 calibration display sequence.
 *
 * @param[in,out] output Attached-wheel display output to update.
 * @param[in] prompt Calibration prompt to render.
 * @param[in] position Position glyph to render for a position prompt.
 * @return True when a glyph changed.
 */
static bool set_calibration_output(WheelDisplayOutput *output, HPatternCalibrationPrompt prompt,
                                   HPatternCalibrationPosition position) {
    uint8_t glyphs[WHEEL_DISPLAY_GLYPH_COUNT] = {output->glyphs[0], output->glyphs[1],
                                                 output->glyphs[2]};
    switch (prompt) {
    case H_PATTERN_CALIBRATION_PROMPT_WAITING:
        glyphs[CALIBRATION_WAITING_FIRST_POSITION] = GLYPH_CALIBRATION_WAITING_FIRST;
        glyphs[CALIBRATION_WAITING_SECOND_POSITION] = GLYPH_CALIBRATION_WAITING_SECOND;
        break;
    case H_PATTERN_CALIBRATION_PROMPT_SHIFTER:
        glyphs[0] = GLYPH_S;
        glyphs[1] = GLYPH_F;
        glyphs[2] = GLYPH_T;
        break;
    case H_PATTERN_CALIBRATION_PROMPT_CALIBRATION:
        glyphs[0] = GLYPH_C;
        glyphs[1] = GLYPH_A;
        glyphs[2] = GLYPH_L;
        break;
    case H_PATTERN_CALIBRATION_PROMPT_POSITION:
        glyphs[CALIBRATION_POSITION_DISPLAY_POSITION] = calibration_glyph(position);
        break;
    case H_PATTERN_CALIBRATION_PROMPT_NONE:
        return false;
    }
    bool changed = output->glyphs[0] != glyphs[0] || output->glyphs[1] != glyphs[1] ||
                   output->glyphs[2] != glyphs[2];
    output->glyphs[0] = glyphs[0];
    output->glyphs[1] = glyphs[1];
    output->glyphs[2] = glyphs[2];
    return changed;
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
 * @brief Requests presentation of the current H-pattern gear.
 *
 * Retains the request until an active attached-wheel display can present the gear, including the
 * neutral glyph.
 *
 * @param[in,out] display Persistent gear display state.
 */
void shifter_display_request_refresh(ShifterDisplay *display) {
    if (display != NULL) {
        display->refresh_requested = true;
        display->refresh_side_effect_pending = true;
    }
}

/**
 * @brief Takes the pending mode-specific refresh side effect.
 *
 * Consumes only the one-shot side effect and leaves the calibration start latch untouched.
 *
 * @param[in,out] display Persistent shifter display state.
 * @return True when a side effect was pending.
 */
bool shifter_display_take_refresh_side_effect(ShifterDisplay *display) {
    if (display == NULL || !display->refresh_side_effect_pending) {
        return false;
    }
    display->refresh_side_effect_pending = false;
    return true;
}

/**
 * @brief Replaces the local OLED shifter presentation when it differs.
 *
 * Compares every semantic field so callers can use the return value as a framebuffer dirty flag.
 *
 * @param[out] output Local presentation to update.
 * @param[in] next New local presentation.
 * @return True when at least one field changed.
 */
static bool set_local_output(ShifterLocalDisplay *output, ShifterLocalDisplay next) {
    bool changed = output->kind != next.kind || output->glyph != next.glyph ||
                   output->calibration_prompt != next.calibration_prompt ||
                   output->calibration_position != next.calibration_position;
    *output = next;
    return changed;
}

/**
 * @brief Advances the local OLED shifter state machine.
 *
 * The local page waits for the first active connection sample, shows changed valid gears for one
 * second, and gives calibration prompts ownership until calibration returns to its idle state.
 *
 * @param[in,out] display Persistent shifter display state.
 * @param[in] gear Current H-pattern gear.
 * @param[in] wheel_active True while the attached-wheel protocol is active.
 * @param[in] h_pattern_available True while an H-pattern input is present.
 * @param[in] calibration_prompt Current calibration prompt.
 * @param[in] calibration_position Next calibration position.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[out] output Local OLED presentation to update.
 * @return True when the presentation changed.
 */
bool shifter_display_update_local(ShifterDisplay *display, ShifterGear gear, bool wheel_active,
                                  bool h_pattern_available,
                                  HPatternCalibrationPrompt calibration_prompt,
                                  HPatternCalibrationPosition calibration_position, uint32_t now_ms,
                                  ShifterLocalDisplay *output) {
    if (display == NULL || output == NULL) {
        return false;
    }
    if (!wheel_active || !h_pattern_available) {
        display->phase = SHIFTER_DISPLAY_WAITING;
        display->calibration_visible = false;
        display->clear_after_ms = 0;
        return set_local_output(output, (ShifterLocalDisplay){0});
    }

    if (calibration_prompt != H_PATTERN_CALIBRATION_PROMPT_NONE) {
        display->phase = SHIFTER_DISPLAY_MONITORING;
        display->last_gear = gear;
        display->clear_after_ms = 0;
        display->calibration_visible = true;
        return set_local_output(
            output, (ShifterLocalDisplay){.kind = SHIFTER_LOCAL_DISPLAY_CALIBRATION,
                                          .calibration_prompt = calibration_prompt,
                                          .calibration_position = calibration_position});
    }

    if (display->calibration_visible) {
        display->calibration_visible = false;
        display->clear_after_ms = 0;
        display->last_gear = gear;
        display->phase = SHIFTER_DISPLAY_MONITORING;
        return set_local_output(output, (ShifterLocalDisplay){0});
    }

    if (display->phase == SHIFTER_DISPLAY_WAITING) {
        display->phase = SHIFTER_DISPLAY_MONITORING;
        display->last_gear = gear;
        return set_local_output(output, (ShifterLocalDisplay){0});
    }

    if (display->phase == SHIFTER_DISPLAY_SHOWING) {
        if (now_ms > display->clear_after_ms || gear == SHIFTER_GEAR_NEUTRAL) {
            display->phase = SHIFTER_DISPLAY_MONITORING;
            display->clear_after_ms = 0;
            return set_local_output(output, (ShifterLocalDisplay){0});
        }
        return false;
    }

    if (gear == display->last_gear) {
        return false;
    }

    display->last_gear = gear;
    uint8_t glyph = gear_glyph(gear);
    if (glyph == 0) {
        return set_local_output(output, (ShifterLocalDisplay){0});
    }
    display->phase = SHIFTER_DISPLAY_SHOWING;
    display->clear_after_ms = now_ms + DISPLAY_HOLD_DURATION_MS;
    return set_local_output(
        output, (ShifterLocalDisplay){.kind = SHIFTER_LOCAL_DISPLAY_GEAR, .glyph = glyph});
}

/**
 * @brief Updates the temporary H-pattern gear display.
 *
 * Calibration owns the glyph display while active, presents the shifter and calibration labels,
 * and then shows the next position to capture. Extended-mode entry waits without replacing its
 * separate presentation. The seventh-gear glyph remains visible for one second after completion.
 * The waiting prompt writes 0x7D and 0x08 to the first two attached-display slots, position prompts
 * write only the third slot, and the SFT and CAL labels replace all three slots. This matches the
 * official calibration sequence at 0x03505A-0x035D74.
 * Outside calibration, a changed non-neutral gear is shown for one second when the display is
 * idle. Phase dispatch handles expiry and neutral clearing before the connection gate. The first
 * active waiting sample records the current gear and returns before rendering, while a monitoring
 * sample is rendered before connection loss returns the service to its waiting phase. This is the
 * official 0x034C78 dispatch: waiting is handled at 0x034C88-0x034CA4, monitoring at
 * 0x034CA6-0x034DA0, showing at 0x034DA2-0x034DC2, and all paths return through 0x034DC8.
 *
 * @param[in,out] display Persistent display phase, last gear, and clear deadline.
 * @param[in] gear Current H-pattern gear, or neutral.
 * @param[in] wheel_active True while the attached-wheel display connection is active.
 * @param[in] calibration_prompt Current H-pattern calibration presentation phase.
 * @param[in] calibration_position Next calibration position, or complete.
 * @param[in] now_ms Current millisecond counter.
 * @param[in,out] output Current display output, updated when a gear is shown or cleared.
 * @return True when the display output changed.
 */
bool shifter_display_update(ShifterDisplay *display, ShifterGear gear, bool wheel_active,
                            HPatternCalibrationPrompt calibration_prompt,
                            HPatternCalibrationPosition calibration_position, uint32_t now_ms,
                            WheelDisplayOutput *output) {
    switch (display->phase) {
    case SHIFTER_DISPLAY_SHOWING:
        if (calibration_prompt == H_PATTERN_CALIBRATION_PROMPT_NONE) {
            if (now_ms > display->clear_after_ms || gear == SHIFTER_GEAR_NEUTRAL) {
                clear_glyphs(output);
                display->phase = SHIFTER_DISPLAY_MONITORING;
                return true;
            }
            return false;
        }
        break;
    case SHIFTER_DISPLAY_WAITING:
        if (!wheel_active) {
            display->phase = SHIFTER_DISPLAY_WAITING;
            display->calibration_visible = false;
            return false;
        }
        display->phase = SHIFTER_DISPLAY_MONITORING;
        display->last_gear = gear;
        return false;
    case SHIFTER_DISPLAY_MONITORING:
        break;
    }

    if (wheel_active && calibration_prompt != H_PATTERN_CALIBRATION_PROMPT_NONE) {
        display->phase = SHIFTER_DISPLAY_MONITORING;
        display->last_gear = gear;
        display->clear_after_ms = 0;
        display->calibration_visible = true;
        return set_calibration_output(output, calibration_prompt, calibration_position);
    }

    if (wheel_active && display->calibration_visible) {
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

    if (wheel_active && display->refresh_requested && display_idle(output)) {
        uint8_t glyph = gear == SHIFTER_GEAR_NEUTRAL ? GLYPH_NEUTRAL : gear_glyph(gear);
        display->refresh_requested = false;
        if (glyph != 0) {
            output->glyphs[GEAR_DISPLAY_POSITION] = glyph;
            display->last_gear = gear;
            display->clear_after_ms = now_ms + DISPLAY_HOLD_DURATION_MS;
            display->phase = SHIFTER_DISPLAY_SHOWING;
            return true;
        }
    }

    bool output_changed = false;
    if (!display->calibration_visible && display->phase == SHIFTER_DISPLAY_MONITORING &&
        gear != display->last_gear) {
        display->last_gear = gear;
        uint8_t glyph = gear_glyph(gear);
        if (glyph != 0 && display_idle(output)) {
            output->glyphs[GEAR_DISPLAY_POSITION] = glyph;
            display->clear_after_ms = now_ms + DISPLAY_HOLD_DURATION_MS;
            display->phase = SHIFTER_DISPLAY_SHOWING;
            output_changed = true;
        }
    }

    if (!wheel_active) {
        display->phase = SHIFTER_DISPLAY_WAITING;
        display->calibration_visible = false;
    }
    return output_changed;
}
