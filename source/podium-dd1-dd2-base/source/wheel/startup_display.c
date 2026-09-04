#include "wheel/startup_display.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "motor/identity.h"
#include "wheel/display_output.h"

/** @brief Startup display timing and segment constants. */
enum {
    BASE_VERSION_DURATION_MS = 1000,           /**< Standard base-version presentation duration. */
    TUNING_DISPLAY_VERSION_DURATION_MS = 3000, /**< Tuning-display version interval. */
    TUNING_DISPLAY_PAGE_DURATION_MS = 3500,    /**< Adapter version-page presentation interval. */
    MOTOR_VERSION_DURATION_MS = 1000,          /**< Motor-version presentation duration. */
    READY_DURATION_MS = 1000,                  /**< Ready-dash presentation duration. */
    CALIBRATION_DURATION_MS = 500,             /**< Calibration-label and blank interval. */
    GLYPH_DASH = 0x40,                         /**< Seven-segment dash glyph. */
    GLYPH_A = 0x77,                            /**< Seven-segment A glyph. */
    GLYPH_C = 0x39,                            /**< Seven-segment C glyph. */
    GLYPH_L = 0x38,                            /**< Seven-segment L glyph. */
    GLYPH_DECIMAL_POINT = 0x80,                /**< Seven-segment decimal-point bit. */
    VERSION_FIRST_COMPONENT_MASK = 0x3f,       /**< Mask for flags in the first version byte. */
};

/**
 * @brief Replaces the three startup glyphs.
 *
 * Copies the supplied glyphs, clears the separate third-position marker, and reports whether the
 * attached-wheel output changed.
 *
 * @param[in,out] output Attached-wheel display output to update.
 * @param[in] first First raw seven-segment glyph.
 * @param[in] second Second raw seven-segment glyph.
 * @param[in] third Third raw seven-segment glyph.
 * @return True when a glyph or marker changed.
 */
static bool set_glyphs(WheelDisplayOutput *output, uint8_t first, uint8_t second, uint8_t third) {
    bool changed = output->glyphs[0] != first || output->glyphs[1] != second ||
                   output->glyphs[2] != third || output->third_glyph_marker;
    output->glyphs[0] = first;
    output->glyphs[1] = second;
    output->glyphs[2] = third;
    output->third_glyph_marker = false;
    return changed;
}

/**
 * @brief Encodes one hexadecimal digit as a raw seven-segment glyph.
 *
 * Maps values zero through F to the official seven-segment table and returns a blank glyph for
 * other values.
 *
 * @param[in] digit Hexadecimal digit to encode.
 * @return Raw seven-segment glyph, or zero for a non-hexadecimal value.
 */
static uint8_t version_glyph(uint8_t digit) {
    static const uint8_t glyphs[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07,
                                     0x7f, 0x6f, 0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71};
    return digit < sizeof(glyphs) ? glyphs[digit] : 0;
}

/**
 * @brief Shows a three-component firmware version.
 *
 * Masks the first component to six bits, maps each component through the official hexadecimal
 * table, and enables the decimal point after the first and second components.
 *
 * @param[in,out] output Attached-wheel display output to update.
 * @param[in] version Four-byte version record whose first three bytes are display components.
 * @return True when the displayed version changed.
 */
static bool show_version(WheelDisplayOutput *output, const uint8_t version[4]) {
    return set_glyphs(output,
                      version_glyph(version[0] & VERSION_FIRST_COMPONENT_MASK) |
                          GLYPH_DECIMAL_POINT,
                      version_glyph(version[1]) | GLYPH_DECIMAL_POINT,
                      version_glyph(version[2]));
}

/**
 * @brief Selects the post-version startup phase.
 *
 * Presents the motor-controller version for standard and position-capable controllers. Otherwise
 * proceeds to the ready delay when position input is available or to calibration when it is not.
 *
 * @param[in,out] display Persistent startup display state.
 * @param[in] motor_identity Identified motor controller, or null when unavailable.
 * @param[in] position_ready True when a valid motor-position report is available.
 */
static void continue_after_version(WheelStartupDisplay *display,
                                   const MotorIdentity *motor_identity, bool position_ready) {
    if (motor_identity != NULL && motor_identity->protocol != MOTOR_PROTOCOL_LEGACY) {
        display->phase = WHEEL_STARTUP_DISPLAY_MOTOR_VERSION;
    } else {
        display->phase =
            position_ready ? WHEEL_STARTUP_DISPLAY_READY_DELAY : WHEEL_STARTUP_DISPLAY_CALIBRATION;
    }
}

/**
 * @brief Initializes the attached-wheel startup glyph sequence.
 *
 * Starts at the three-dash presentation with no deadline and keeps normal display owners blocked.
 *
 * @param[out] display Startup display state to initialize.
 */
void wheel_startup_display_init(WheelStartupDisplay *display) {
    *display = (WheelStartupDisplay){0};
}

/**
 * @brief Advances the attached-wheel startup glyph sequence.
 *
 * Waits for an active wheel, presents three dashes, the base firmware version, an available managed
 * motor version, and a one-second ready delay. Wheels with a tuning display retain the dash glyphs
 * while three seconds are reserved for their separate version presentation. Missing position input
 * alternates the CAL label and a blank display every 500 milliseconds until position becomes
 * available. Completion clears the glyphs and releases normal display owners.
 *
 * @param[in,out] display Persistent startup phase, deadline, and readiness state.
 * @param[in] wheel_active True while the attached-wheel protocol is active.
 * @param[in] tuning_display_supported True when the wheel supplies its own tuning display.
 * @param[in] position_ready True when a valid motor-position report is available.
 * @param[in] motor_identity Identified motor controller, or null when unavailable.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in,out] output Attached-wheel glyph output updated by the active presentation.
 * @return True when the glyph output changed.
 */
bool wheel_startup_display_update(WheelStartupDisplay *display, bool wheel_active,
                                  bool tuning_display_supported, bool position_ready,
                                  const MotorIdentity *motor_identity, uint32_t now_ms,
                                  WheelDisplayOutput *output) {
    if (!wheel_active || display->phase == WHEEL_STARTUP_DISPLAY_COMPLETE) {
        return false;
    }
    if (display->version_presentation_close_armed &&
        (int32_t)(now_ms - display->version_presentation_close_ms) > 0) {
        display->version_presentation_close_armed = false;
        display->version_presentation_close_pending = true;
    }

    switch (display->phase) {
    case WHEEL_STARTUP_DISPLAY_DASHES: {
        bool changed = set_glyphs(output, GLYPH_DASH, GLYPH_DASH, GLYPH_DASH);
        if (now_ms <= display->deadline_ms) {
            return changed;
        }
        display->deadline_ms =
            now_ms + (tuning_display_supported ? TUNING_DISPLAY_VERSION_DURATION_MS
                                               : BASE_VERSION_DURATION_MS);
        display->version_presentation_pending = tuning_display_supported;
        display->version_presentation_close_armed = tuning_display_supported;
        display->version_presentation_close_ms = now_ms + TUNING_DISPLAY_PAGE_DURATION_MS;
        display->phase = WHEEL_STARTUP_DISPLAY_BASE_VERSION;
        return changed;
    }
    case WHEEL_STARTUP_DISPLAY_BASE_VERSION: {
        /** @brief Base firmware version shown during startup. */
        static const uint8_t version[4] = {3, 9, 1, 1};
        bool changed = tuning_display_supported ? false : show_version(output, version);
        if (now_ms < display->deadline_ms) {
            return changed;
        }
        display->deadline_ms = now_ms + MOTOR_VERSION_DURATION_MS;
        continue_after_version(display, motor_identity, position_ready);
        return changed;
    }
    case WHEEL_STARTUP_DISPLAY_MOTOR_VERSION: {
        bool changed =
            tuning_display_supported ? false : show_version(output, motor_identity->version);
        if (now_ms <= display->deadline_ms) {
            return changed;
        }
        display->deadline_ms = now_ms + READY_DURATION_MS;
        display->phase =
            position_ready ? WHEEL_STARTUP_DISPLAY_READY_DELAY : WHEEL_STARTUP_DISPLAY_CALIBRATION;
        return changed;
    }
    case WHEEL_STARTUP_DISPLAY_READY_DELAY: {
        bool changed = set_glyphs(output, GLYPH_DASH, GLYPH_DASH, GLYPH_DASH);
        if (now_ms <= display->deadline_ms) {
            return changed;
        }
        changed |= set_glyphs(output, 0, 0, 0);
        display->phase = WHEEL_STARTUP_DISPLAY_COMPLETE;
        display->ready = true;
        return changed;
    }
    case WHEEL_STARTUP_DISPLAY_CALIBRATION: {
        bool changed = set_glyphs(output, GLYPH_C, GLYPH_A, GLYPH_L);
        if (now_ms <= display->deadline_ms) {
            return changed;
        }
        changed |= set_glyphs(output, 0, 0, 0);
        if (position_ready) {
            display->phase = WHEEL_STARTUP_DISPLAY_COMPLETE;
            display->ready = true;
        } else {
            display->deadline_ms = now_ms + CALIBRATION_DURATION_MS;
            display->phase = WHEEL_STARTUP_DISPLAY_CALIBRATION_PAUSE;
        }
        return changed;
    }
    case WHEEL_STARTUP_DISPLAY_CALIBRATION_PAUSE: {
        bool changed = set_glyphs(output, 0, 0, 0);
        if (now_ms <= display->deadline_ms) {
            return changed;
        }
        display->deadline_ms = now_ms + CALIBRATION_DURATION_MS;
        display->phase = WHEEL_STARTUP_DISPLAY_CALIBRATION;
        return changed;
    }
    case WHEEL_STARTUP_DISPLAY_COMPLETE:
        return false;
    }
    return false;
}

/**
 * @brief Reports whether normal attached-wheel display ownership is available.
 *
 * Returns the retained completion state without advancing the startup sequence.
 *
 * @param[in] display Persistent startup display state.
 * @return True after the startup presentation completes.
 */
bool wheel_startup_display_ready(const WheelStartupDisplay *display) { return display->ready; }

/**
 * @brief Takes the pending tuning-display version presentation request.
 *
 * Returns one request when the startup sequence enters its three-second tuning-display interval,
 * then clears it so the presentation is not queued again.
 *
 * @param[in,out] display Persistent startup display state.
 * @return True once when a tuning-display version presentation is due.
 */
bool wheel_startup_display_take_version_presentation(WheelStartupDisplay *display) {
    bool pending = display->version_presentation_pending;
    display->version_presentation_pending = false;
    return pending;
}

/**
 * @brief Takes the pending tuning-display version close request.
 *
 * Returns one request after the 3.5-second tuning-display version interval and clears it so the
 * close action is not queued again.
 *
 * @param[in,out] display Persistent startup display state.
 * @return True once when the tuning-display version page is due to close.
 */
bool wheel_startup_display_take_version_presentation_close(WheelStartupDisplay *display) {
    bool pending = display->version_presentation_close_pending;
    display->version_presentation_close_pending = false;
    return pending;
}
