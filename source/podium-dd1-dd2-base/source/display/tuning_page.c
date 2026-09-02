#include "display/tuning_page.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "display/framebuffer.h"
#include "display/text.h"
#include "profile/bank.h"
#include "profile/tuning.h"
#include "profile/tuning_entry.h"
#include "profile/tuning_menu.h"

/**
 * @brief Defines tuning-page colors and record coordinates.
 *
 * These constants control the fixed layout used to render the local OLED tuning page.
 */
enum {
    PAGE_COLOR = 15,                /**< Foreground grayscale value for tuning-page content. */
    PAGE_ENTRY_X = 13,              /**< Entry-abbreviation leading column. */
    PAGE_ENTRY_Y = 13,              /**< Entry-abbreviation top row. */
    PAGE_SETUP_LABEL_X = 0,         /**< Setup-label leading column. */
    PAGE_SETUP_LABEL_Y = 50,        /**< Setup-label top row. */
    PAGE_PRIMARY_X = 30,            /**< Primary value leading column. */
    PAGE_PRIMARY_Y = 30,            /**< Primary value top row. */
    PAGE_HELP_X = 31,               /**< Primary-help leading column. */
    PAGE_HELP_Y = 48,               /**< Primary-help top row. */
    PAGE_PROGRESS_LEFT = 30,        /**< Progress-bar left coordinate. */
    PAGE_PROGRESS_TOP = 47,         /**< Progress-bar top coordinate. */
    PAGE_PROGRESS_RIGHT = 225,      /**< Progress-bar right coordinate. */
    PAGE_PROGRESS_BOTTOM = 62,      /**< Progress-bar bottom coordinate. */
    PAGE_NAV_LEFT = 30,             /**< Navigation-bar left coordinate. */
    PAGE_NAV_RIGHT = 225,           /**< Navigation-bar right coordinate. */
    PAGE_NAV_Y = 62,                /**< Navigation-bar row. */
    PAGE_NAV_MARKER_LEFT = 12,      /**< Navigation marker left coordinate. */
    PAGE_NAV_MARKER_RIGHT = 16,     /**< Navigation marker right coordinate. */
    PAGE_NAV_MARKER_Y = 58,         /**< Navigation marker row. */
    PAGE_NAV_VERTICAL_X = 16,       /**< Navigation guide x coordinate. */
    PAGE_NAV_VERTICAL_TOP = 19,     /**< Navigation guide top row. */
    PAGE_NAV_VERTICAL_BOTTOM = 58,  /**< Navigation guide bottom row. */
    PAGE_NAV_HORIZONTAL_LEFT = 16,  /**< Navigation guide left coordinate. */
    PAGE_NAV_HORIZONTAL_RIGHT = 30, /**< Navigation guide right coordinate. */
    PAGE_NAV_HORIZONTAL_Y = 19,     /**< Navigation guide row. */
    PAGE_ENTRY_GUIDE_X = 29,        /**< Entry-to-navigation guide x coordinate. */
    PAGE_ENTRY_GUIDE_TOP = 30,      /**< Entry-to-navigation guide top row. */
    PAGE_ENTRY_GUIDE_BOTTOM = 63,   /**< Entry-to-navigation guide end coordinate. */
    PAGE_CAUTION_X = 105,           /**< Conditional caution-help leading column. */
    PAGE_CAUTION_FIRST_Y = 25,      /**< First conditional caution row. */
    PAGE_CAUTION_SECOND_Y = 35,     /**< Second conditional caution row. */
};

/**
 * @brief Stores catalog metadata for one tuning entry.
 *
 * The metadata supplies the short label, title, and description displayed for an entry.
 */
typedef struct {
    const char *label;       /**< Short entry label. */
    const char *title;       /**< Entry title. */
    const char *description; /**< Entry description. */
} TuningPageMetadata;

/**
 * @brief Contains display metadata for every tuning entry.
 *
 * Entries are indexed by TuningEntry values so page content can be selected without changing the
 * profile data.
 */
static const TuningPageMetadata page_metadata[TUNING_ENTRY_COUNT] = {
    [TUNING_ENTRY_SETUP] = {"SET", "Auto Setup", "Values set by game or default"},
    [TUNING_ENTRY_SENSITIVITY] = {"SEN", "Sensitivity", "Steering range in degrees"},
    [TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH] = {"FFB", "Force Feedback Str.",
                                              "Overall maximum Force Feedback Strength"},
    [TUNING_ENTRY_VIBRATION_STRENGTH] = {"SHO", "Vibration",
                                         "Toggle: Wheel vibration motor On/Off"},
    [TUNING_ENTRY_BRAKE_INDICATOR_LEVEL] = {"BLI", "Brake Level Indicator",
                                            "Required brake input % to trigger vibration"},
    [TUNING_ENTRY_FORCE_SCALE] = {"FFS", "FF Scale", "Force Feedback scaling mode"},
    [TUNING_ENTRY_STEERING_DEADZONE] = {"DEA", "Deadzone", "Steering angle deadzone"},
    [TUNING_ENTRY_DRIFT_COMPENSATION] = {"DRI", "Drift Mode",
                                         "Drift mode - Damping of the wheel axis"},
    [TUNING_ENTRY_FORCE_EFFECT_STRENGTH] = {"FOR", "Force Effect Str.",
                                            "Force Effect Strength modifier"},
    [TUNING_ENTRY_SPRING_EFFECT_STRENGTH] = {"SPR", "Spring Effect Str.",
                                             "Spring Effect Strength modifier"},
    [TUNING_ENTRY_DAMPER_EFFECT_STRENGTH] = {"DPR", "Damper Effect Str.",
                                             "Damper Effect Strength modifier"},
    [TUNING_ENTRY_NATURAL_DAMPER] = {"NDP", "Natural Damper",
                                     "Mechanical wheel axis damping simulation"},
    [TUNING_ENTRY_NATURAL_FRICTION] = {"NFR", "Natural Friction",
                                       "Mechanical wheel axis friction simulation"},
    [TUNING_ENTRY_BRAKE_FORCE] = {"BRF", "Brake Force", "Adjust load cell sensitivity"},
    [TUNING_ENTRY_ALTERNATE_BRAKE_FORCE] = {"BRF", "Brake Force", "Adjust load cell sensitivity"},
    [TUNING_ENTRY_FORCE_EFFECT_INTENSITY] = {"FEI", "FE Intensity",
                                             "Force Effect spikes intensity modifier"},
    [TUNING_ENTRY_MULTI_POSITION_MODE] = {"MPS", "MPS Mode", "Multi-Position Switch mode selector"},
    [TUNING_ENTRY_PADDLE_MODE] = {"ACP", "ACP Mode", "Analogue Clutch Paddle mode selector"},
    [TUNING_ENTRY_INTERPOLATION_FILTER] = {"INT", "FFB Interpolation",
                                           "FFB Interpolation Filter level"},
    [TUNING_ENTRY_NATURAL_INERTIA] = {"NIN", "Natural Inertia",
                                      "Mechanical wheel axis inertia simulation"},
    [TUNING_ENTRY_FULL_FORCE] = {"FUL", "FullForce Effect Str.",
                                 "FullForce Effect Strength modifier"},
    [TUNING_ENTRY_BUTTON_ILLUMINATION] = {"BIL", "Button Illumination", "Set Button Illumination"},
    [TUNING_ENTRY_DISPLAY_ROTATION] = {"ROT", "Display Rotation", "Set Display Rotation"},
    [TUNING_ENTRY_BRAKE_PEDAL_CURVE] = {"BPC", "Brake Pedal Character.",
                                        "Brake pedal input curve adjustment"},
    [TUNING_ENTRY_CLUTCH_PEDAL_CURVE] = {"CPC", "Clutch Pedal Character.",
                                         "Clutch pedal input curve adjustment"},
    [TUNING_ENTRY_THROTTLE_PEDAL_CURVE] = {"TPC", "Throttle Pedal Character.",
                                           "Throttle pedal input curve adjustment"},
};

/**
 * @brief Copies display text into a bounded value field.
 *
 * Always terminates the destination and truncates input that cannot fit.
 *
 * @param[out] destination Value field receiving text.
 * @param[in] size Available destination bytes.
 * @param[in] source Null-terminated source text.
 */
static void copy_text(char *destination, size_t size, const char *source) {
    size_t index = 0;
    if (size == 0) {
        return;
    }
    while (index + 1 < size && source[index] != '\0') {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
}

/**
 * @brief Formats an unsigned tuning value as decimal text.
 *
 * Emits the shortest decimal representation, including a single zero digit.
 *
 * @param[out] destination Value field receiving the number.
 * @param[in] size Available destination bytes.
 * @param[in] value Unsigned value to format.
 */
static void format_unsigned(char *destination, size_t size, uint16_t value) {
    char reversed[5];
    uint8_t length = 0;
    do {
        reversed[length++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0 && length < sizeof(reversed));

    uint8_t index = 0;
    while (index < length && (size_t)index + 1 < size) {
        destination[index] = reversed[length - index - 1];
        index++;
    }
    if (size != 0) {
        destination[index] = '\0';
    }
}

/**
 * @brief Formats a tuning level with its disabled state.
 *
 * Shows `OFF` for zero and a scaled decimal value for an enabled level.
 *
 * @param[out] destination Value field receiving the result.
 * @param[in] size Available destination bytes.
 * @param[in] value Stored tuning level.
 * @param[in] scale Visible multiplier for an enabled level.
 */
static void format_level(char *destination, size_t size, uint8_t value, uint8_t scale) {
    if (value == 0) {
        copy_text(destination, size, "OFF");
    } else {
        format_unsigned(destination, size, (uint16_t)value * scale);
    }
}

/**
 * @brief Formats the selected setup name.
 *
 * Distinguishes automatic setup, Standard custom setup, and numbered Advanced setups.
 *
 * @param[out] destination Value field receiving the setup name.
 * @param[in] size Available destination bytes.
 * @param[in] bank Current tuning profile bank.
 */
static void format_setup(char *destination, size_t size, const TuningProfileBank *bank) {
    if (bank->selected_slot == 0) {
        copy_text(destination, size, "Auto Setup");
        return;
    }
    if (bank->standard_mode_enabled && bank->selected_slot == 1) {
        copy_text(destination, size, "Custom Setup");
        return;
    }
    copy_text(destination, size, "Setup ");
    format_unsigned(destination + 6, size > 6 ? size - 6 : 0, bank->selected_slot);
}

/**
 * @brief Formats the selected local tuning value.
 *
 * Applies the entry-specific automatic, disabled, scaled, limit, mode, flag, and curve text used
 * by the base tuning interface.
 *
 * @param[out] destination Value field receiving the presentation.
 * @param[in] size Available destination bytes.
 * @param[in] entry Selected logical tuning entry.
 * @param[in] bank Current tuning profile bank.
 * @param[in] profile Selected tuning profile.
 */
static void format_value(char *destination, size_t size, TuningEntry entry,
                         const TuningProfileBank *bank, const TuningProfile *profile) {
    /** @brief Display names for multi-position switch modes. */
    static const char *const multi_position_modes[] = {"Encoder", "Pulse", "Constant", "AUTO"};
    /** @brief Display names for analogue paddle modes. */
    static const char *const paddle_modes[] = {"", "Clutch Bite Point", "Clutch + Handbrake",
                                               "Brake + Throttle", "Analogue Axes"};
    /** @brief Display names for pedal response curves. */
    static const char *const pedal_curves[] = {"Custom 1", "Custom 2",     "Custom 3",
                                               "Linear ",  "Progressive ", "Degressive "};

    switch (entry) {
    case TUNING_ENTRY_SETUP:
        format_setup(destination, size, bank);
        break;
    case TUNING_ENTRY_SENSITIVITY:
        if (profile->automatic_rotation != 0) {
            copy_text(destination, size, "AUTO");
        } else {
            format_unsigned(destination, size, profile->rotation_degrees);
        }
        break;
    case TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH:
        if (profile->force_feedback_strength == 101) {
            copy_text(destination, size, "AUTO");
        } else {
            format_level(destination, size, profile->force_feedback_strength, 1);
        }
        break;
    case TUNING_ENTRY_VIBRATION_STRENGTH:
        copy_text(destination, size, profile->vibration_strength == 0 ? "OFF" : "ON ");
        break;
    case TUNING_ENTRY_BRAKE_INDICATOR_LEVEL:
        if (profile->brake_indicator_level > 100) {
            copy_text(destination, size, "OFF");
        } else {
            format_unsigned(destination, size, profile->brake_indicator_level);
        }
        break;
    case TUNING_ENTRY_FORCE_SCALE:
        copy_text(destination, size,
                  profile->force_scale == TUNING_FORCE_SCALE_PEAK ? "PEAK" : "LINEAR");
        break;
    case TUNING_ENTRY_STEERING_DEADZONE:
        format_level(destination, size, profile->steering_deadzone, 10);
        break;
    case TUNING_ENTRY_DRIFT_COMPENSATION:
        format_level(destination, size, profile->drift_compensation, 1);
        break;
    case TUNING_ENTRY_FORCE_EFFECT_STRENGTH:
        format_level(destination, size, profile->force_effect_strength, 10);
        break;
    case TUNING_ENTRY_SPRING_EFFECT_STRENGTH:
        format_level(destination, size, profile->spring_effect_strength, 10);
        break;
    case TUNING_ENTRY_DAMPER_EFFECT_STRENGTH:
        format_level(destination, size, profile->damper_effect_strength, 10);
        break;
    case TUNING_ENTRY_NATURAL_DAMPER:
        format_level(destination, size, profile->natural_damper, 1);
        break;
    case TUNING_ENTRY_NATURAL_FRICTION:
        format_level(destination, size, profile->natural_friction, 1);
        break;
    case TUNING_ENTRY_BRAKE_FORCE:
    case TUNING_ENTRY_ALTERNATE_BRAKE_FORCE: {
        uint8_t force = entry == TUNING_ENTRY_BRAKE_FORCE ? profile->brake_force
                                                          : profile->alternate_brake_force;
        if (force == 0) {
            copy_text(destination, size, "MIN.");
        } else if (force == 100) {
            copy_text(destination, size, "MAX.");
        } else {
            format_unsigned(destination, size, force);
        }
        break;
    }
    case TUNING_ENTRY_FORCE_EFFECT_INTENSITY:
        format_unsigned(destination, size, profile->force_effect_intensity);
        break;
    case TUNING_ENTRY_MULTI_POSITION_MODE:
        copy_text(
            destination, size,
            multi_position_modes[profile->multi_position_mode <= TUNING_MULTI_POSITION_AUTOMATIC
                                     ? profile->multi_position_mode
                                     : TUNING_MULTI_POSITION_ENCODER]);
        break;
    case TUNING_ENTRY_PADDLE_MODE:
        if (profile->paddle_mode >= TUNING_CLUTCH_BRAKE &&
            profile->paddle_mode <= TUNING_DUAL_ANALOG) {
            copy_text(destination, size, paddle_modes[profile->paddle_mode]);
        } else {
            format_unsigned(destination, size, profile->paddle_mode);
        }
        break;
    case TUNING_ENTRY_INTERPOLATION_FILTER:
        format_level(destination, size, profile->interpolation_filter, 1);
        break;
    case TUNING_ENTRY_NATURAL_INERTIA:
        format_level(destination, size, profile->natural_inertia, 1);
        break;
    case TUNING_ENTRY_FULL_FORCE:
        copy_text(destination, size, profile->full_force_enabled != 0 ? "On" : "Off");
        break;
    case TUNING_ENTRY_BUTTON_ILLUMINATION:
        copy_text(destination, size, profile->button_illumination_enabled != 0 ? "On" : "Off");
        break;
    case TUNING_ENTRY_DISPLAY_ROTATION:
        copy_text(destination, size, profile->display_rotation_enabled != 0 ? "On" : "Off");
        break;
    case TUNING_ENTRY_BRAKE_PEDAL_CURVE:
    case TUNING_ENTRY_CLUTCH_PEDAL_CURVE:
    case TUNING_ENTRY_THROTTLE_PEDAL_CURVE: {
        TuningPedalCurve curve =
            entry == TUNING_ENTRY_BRAKE_PEDAL_CURVE    ? profile->brake_pedal_curve
            : entry == TUNING_ENTRY_CLUTCH_PEDAL_CURVE ? profile->clutch_pedal_curve
                                                       : profile->throttle_pedal_curve;
        copy_text(
            destination, size,
            pedal_curves[curve <= TUNING_PEDAL_CURVE_DEGREES ? curve : TUNING_PEDAL_CURVE_ONE]);
        break;
    }
    default:
        destination[0] = '\0';
        break;
    }
}

/**
 * @brief Draws one exclusive-end horizontal display span.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] first_x First column.
 * @param[in] end_x Exclusive end column.
 * @param[in] y Row.
 * @param[in] color Four-bit grayscale value.
 */
static void draw_horizontal_span(DisplayFramebuffer framebuffer, uint16_t first_x, uint16_t end_x,
                                 uint16_t y, uint8_t color) {
    for (uint16_t x = first_x; x < end_x; x++) {
        display_framebuffer_set_pixel(framebuffer, x, y, color);
    }
}

/**
 * @brief Draws one exclusive-end vertical display span.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] x Column.
 * @param[in] first_y First row.
 * @param[in] end_y Exclusive end row.
 * @param[in] color Four-bit grayscale value.
 */
static void draw_vertical_span(DisplayFramebuffer framebuffer, uint16_t x, uint16_t first_y,
                               uint16_t end_y, uint8_t color) {
    for (uint16_t y = first_y; y < end_y; y++) {
        display_framebuffer_set_pixel(framebuffer, x, y, color);
    }
}

/**
 * @brief Draws the entry-to-navigation guide record used by the tuning menu.
 *
 * These spans are the records initialized at binary address 0x01f028. Their exclusive end
 * coordinates match the reference display driver.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 */
static void draw_navigation_guide(DisplayFramebuffer framebuffer) {
    draw_vertical_span(framebuffer, PAGE_ENTRY_GUIDE_X, PAGE_ENTRY_GUIDE_TOP,
                       PAGE_ENTRY_GUIDE_BOTTOM, 8);
}

/**
 * @brief Draws the fixed navigation spans used by the tuning menu.
 *
 * These spans are the records initialized at binary address 0x01f028. Their exclusive end
 * coordinates match the reference display driver.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 */
static void draw_navigation_spans(DisplayFramebuffer framebuffer) {
    draw_horizontal_span(framebuffer, PAGE_NAV_LEFT, PAGE_NAV_RIGHT, PAGE_NAV_Y, 8);
    draw_horizontal_span(framebuffer, PAGE_NAV_MARKER_LEFT, PAGE_NAV_MARKER_RIGHT,
                         PAGE_NAV_MARKER_Y, 8);
    draw_vertical_span(framebuffer, PAGE_NAV_VERTICAL_X, PAGE_NAV_VERTICAL_TOP,
                       PAGE_NAV_VERTICAL_BOTTOM, 8);
    draw_horizontal_span(framebuffer, PAGE_NAV_HORIZONTAL_LEFT, PAGE_NAV_HORIZONTAL_RIGHT,
                         PAGE_NAV_HORIZONTAL_Y, 8);
}

/**
 * @brief Maps a tuning value to the official zero-through-100 progress value.
 *
 * The reference forces a one-pixel fill at the minimum, so a valid numeric entry never renders an
 * entirely empty bar.
 *
 * @param[in] entry Selected tuning entry.
 * @param[in] profile Selected profile.
 * @return Progress value from one through 100.
 */
static uint8_t progress_value(TuningEntry entry, const TuningProfile *profile) {
    int32_t value;
    int32_t minimum;
    int32_t maximum;
    switch (entry) {
    case TUNING_ENTRY_SENSITIVITY:
        value = profile->rotation_degrees;
        minimum = TUNING_ROTATION_MIN_DEGREES;
        maximum = TUNING_ROTATION_MAX_DEGREES;
        break;
    case TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH:
        value = profile->force_feedback_strength;
        minimum = 0;
        maximum = 100;
        break;
    case TUNING_ENTRY_BRAKE_INDICATOR_LEVEL:
        value = profile->brake_indicator_level;
        minimum = 1;
        maximum = 101;
        break;
    case TUNING_ENTRY_STEERING_DEADZONE:
        value = profile->steering_deadzone;
        minimum = 0;
        maximum = 10;
        break;
    case TUNING_ENTRY_DRIFT_COMPENSATION:
        value = profile->drift_compensation;
        minimum = 0;
        maximum = 1;
        break;
    case TUNING_ENTRY_FORCE_EFFECT_STRENGTH:
        value = profile->force_effect_strength;
        minimum = 0;
        maximum = 12;
        break;
    case TUNING_ENTRY_SPRING_EFFECT_STRENGTH:
        value = profile->spring_effect_strength;
        minimum = 0;
        maximum = 12;
        break;
    case TUNING_ENTRY_DAMPER_EFFECT_STRENGTH:
        value = profile->damper_effect_strength;
        minimum = 0;
        maximum = 12;
        break;
    case TUNING_ENTRY_NATURAL_DAMPER:
        value = profile->natural_damper;
        minimum = 0;
        maximum = 100;
        break;
    case TUNING_ENTRY_NATURAL_FRICTION:
        value = profile->natural_friction;
        minimum = 0;
        maximum = 100;
        break;
    case TUNING_ENTRY_BRAKE_FORCE:
        value = profile->brake_force;
        minimum = 0;
        maximum = 100;
        break;
    case TUNING_ENTRY_ALTERNATE_BRAKE_FORCE:
        value = profile->alternate_brake_force;
        minimum = 0;
        maximum = 100;
        break;
    case TUNING_ENTRY_FORCE_EFFECT_INTENSITY:
        value = profile->force_effect_intensity;
        minimum = 0;
        maximum = 100;
        break;
    case TUNING_ENTRY_INTERPOLATION_FILTER:
        value = profile->interpolation_filter;
        minimum = 0;
        maximum = 20;
        break;
    case TUNING_ENTRY_NATURAL_INERTIA:
        value = profile->natural_inertia;
        minimum = 0;
        maximum = 100;
        break;
    case TUNING_ENTRY_FULL_FORCE:
        value = profile->full_force_enabled;
        minimum = 0;
        maximum = 1;
        break;
    case TUNING_ENTRY_BUTTON_ILLUMINATION:
        value = profile->button_illumination_enabled;
        minimum = 0;
        maximum = 1;
        break;
    case TUNING_ENTRY_DISPLAY_ROTATION:
        value = profile->display_rotation_enabled;
        minimum = 0;
        maximum = 1;
        break;
    default:
        return 0;
    }
    if (value < minimum) {
        value = minimum;
    } else if (value > maximum) {
        value = maximum;
    }
    uint8_t result = (uint8_t)(((value - minimum) * 100) / (maximum - minimum));
    return result == 0 ? 1 : result;
}

/**
 * @brief Reports whether the selected entry owns a progress record.
 *
 * @param[in] entry Selected tuning entry.
 * @param[in] view Current menu view.
 * @return true when no progress bar is rendered.
 */
static bool tuning_entry_hides_progress(TuningEntry entry, TuningMenuView view) {
    if (view == TUNING_MENU_VIEW_LABEL) {
        return true;
    }
    switch (entry) {
    case TUNING_ENTRY_SETUP:
    case TUNING_ENTRY_VIBRATION_STRENGTH:
    case TUNING_ENTRY_FORCE_SCALE:
    case TUNING_ENTRY_MULTI_POSITION_MODE:
    case TUNING_ENTRY_PADDLE_MODE:
    case TUNING_ENTRY_BRAKE_PEDAL_CURVE:
    case TUNING_ENTRY_CLUTCH_PEDAL_CURVE:
    case TUNING_ENTRY_THROTTLE_PEDAL_CURVE:
        return true;
    default:
        return false;
    }
}

/**
 * @brief Draws the borderless left-to-right tuning progress record.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] value Progress percentage.
 */
static void draw_progress(DisplayFramebuffer framebuffer, uint8_t value) {
    if (value > 100) {
        value = 100;
    }
    uint16_t filled = (uint16_t)((PAGE_PROGRESS_RIGHT - PAGE_PROGRESS_LEFT) * value / 100u);
    for (uint16_t y = PAGE_PROGRESS_TOP; y < PAGE_PROGRESS_BOTTOM; y++) {
        draw_horizontal_span(framebuffer, PAGE_PROGRESS_LEFT,
                             (uint16_t)(PAGE_PROGRESS_LEFT + filled), y, 2);
    }
}

/**
 * @brief Draws the reference value-dependent help lines.
 *
 * The binary publishes these lines at x105/y25 and x105/y35. Empty lines are skipped.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] entry Selected tuning entry.
 * @param[in] profile Selected profile.
 */
static void draw_value_help(DisplayFramebuffer framebuffer, TuningEntry entry,
                            const TuningProfile *profile) {
    const char *first = NULL;
    const char *second = NULL;
    switch (entry) {
    case TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH:
        if (profile->force_feedback_strength == 0) {
            first = "CAUTION:";
            second = "FFB disabled.";
        } else if (profile->force_feedback_strength >= 76 &&
                   profile->force_feedback_strength <= 100) {
            first = "CAUTION:";
            second = "High FFB can be dangerous!";
        }
        break;
    case TUNING_ENTRY_BRAKE_INDICATOR_LEVEL:
        if (profile->brake_indicator_level == 0) {
            first = "CAUTION:";
            second = "Will cause constant vibration!";
        } else if (profile->brake_indicator_level > 100) {
            first = "Only the game will trigger";
            second = "brake pedal vibration.";
        } else {
            first = "Amount of brake input";
            second = "needed to trigger rumble.";
        }
        break;
    case TUNING_ENTRY_FORCE_EFFECT_STRENGTH:
        if (profile->force_effect_strength == 0) {
            first = "Force effects disabled.";
        } else if (profile->force_effect_strength >= 11) {
            first = "CAUTION:";
            second = "Will cause clipping!";
        }
        break;
    case TUNING_ENTRY_SPRING_EFFECT_STRENGTH:
        if (profile->spring_effect_strength == 0) {
            first = "Spring effects disabled.";
        } else if (profile->spring_effect_strength >= 11) {
            first = "CAUTION:";
            second = "Will cause clipping!";
        }
        break;
    case TUNING_ENTRY_DAMPER_EFFECT_STRENGTH:
        if (profile->damper_effect_strength == 0) {
            first = "Damper effects disabled.";
        } else if (profile->damper_effect_strength >= 11) {
            first = "CAUTION:";
            second = "Will cause clipping!";
        }
        break;
    case TUNING_ENTRY_NATURAL_DAMPER:
        if (profile->natural_damper <= 14) {
            first = "CAUTION:";
            second = "Low NDP can be dangerous!";
        } else if (profile->natural_damper <= 50) {
            first = "NDP can add realistic feeling";
            second = "and prevent oscillation.";
        } else {
            first = "High NDP slows down reaction";
            second = "and reduces FFB detail.";
        }
        break;
    case TUNING_ENTRY_NATURAL_FRICTION:
        if (profile->natural_friction <= 30) {
            first = "NFR can add realistic feeling";
            second = "and prevent oscillation.";
        } else {
            first = "High NFR slows down reaction";
            second = "and reduces FFB detail.";
        }
        break;
    case TUNING_ENTRY_FORCE_EFFECT_INTENSITY:
        if (profile->force_effect_intensity <= 20) {
            first = "Low FEI reduces FFB detail.";
        } else if (profile->force_effect_intensity < 100) {
            first = "Lower settings smoothen FFB.";
        } else {
            first = "1:1 FFB from the game.";
        }
        break;
    case TUNING_ENTRY_INTERPOLATION_FILTER:
        if (profile->interpolation_filter == 0) {
            first = "Unfiltered, raw FFB signal.";
            second = "Can feel noisy and rough.";
        } else if (profile->interpolation_filter <= 8) {
            first = "Interpolation enabled.";
            second = "Fast, smooth FFB signal.";
        } else {
            first = "High interpolation can reduce";
            second = "detail and response.";
        }
        break;
    case TUNING_ENTRY_NATURAL_INERTIA:
        if (profile->natural_inertia <= 20) {
            first = "Adds weighted feel and can";
            second = "reduce initial oscillation.";
        } else {
            first = "High NIN slows down reaction";
            second = "and can increase oscillation.";
        }
        break;
    case TUNING_ENTRY_BRAKE_FORCE:
    case TUNING_ENTRY_ALTERNATE_BRAKE_FORCE: {
        uint8_t value = entry == TUNING_ENTRY_BRAKE_FORCE ? profile->brake_force
                                                          : profile->alternate_brake_force;
        if (value == 0) {
            first = "Minimum force needed.";
        } else if (value < 50) {
            first = "Less braking force needed.";
        } else if (value < 100) {
            first = "More braking force needed.";
        } else {
            first = "Maximum force needed.";
        }
        break;
    }
    case TUNING_ENTRY_VIBRATION_STRENGTH:
        first = "Wheel vibration motors";
        second = profile->vibration_strength == 0 ? "disabled" : "enabled";
        break;
    case TUNING_ENTRY_FORCE_SCALE:
        if (profile->force_scale == TUNING_FORCE_SCALE_PEAK) {
            first = "   Allows maximum";
            second = "   peak torque output.";
        } else {
            first = "   Maintains consistent and";
            second = "   reliable torque output.";
        }
        break;
    default:
        break;
    }
    if (first != NULL) {
        display_text_draw_with_font(framebuffer, &display_font_10_00c988, first, PAGE_CAUTION_X,
                                    PAGE_CAUTION_FIRST_Y, false);
    }
    if (second != NULL) {
        display_text_draw_with_font(framebuffer, &display_font_10_00c988, second, PAGE_CAUTION_X,
                                    PAGE_CAUTION_SECOND_Y, false);
    }
}

/**
 * @brief Builds the text for one local OLED tuning page.
 *
 * Selects the directly visible catalog title and description and formats the selected setup's
 * current logical value. A closed or invalid menu does not own the page.
 *
 * @param[in] menu Current local tuning selection and visible side.
 * @param[in] bank Current tuning profile bank.
 * @param[out] content Page text receiving the presentation.
 * @return True when a valid local tuning page was built.
 */
bool display_tuning_page_present(const TuningMenu *menu, const TuningProfileBank *bank,
                                 TuningPageContent *content) {
    if (menu == NULL || bank == NULL || content == NULL ||
        menu->selected_entry >= TUNING_ENTRY_COUNT) {
        return false;
    }
    const TuningProfile *profile = tuning_profile_bank_selected(bank);
    if (profile == NULL) {
        return false;
    }

    const TuningPageMetadata *metadata = &page_metadata[menu->selected_entry];
    content->label = metadata->label;
    content->title = metadata->title;
    content->description = metadata->description;
    format_value(content->value, sizeof(content->value), menu->selected_entry, bank, profile);

    if (menu->selected_entry == TUNING_ENTRY_SETUP) {
        content->title = content->value;
        content->description =
            bank->selected_slot == 0 ? "Values set by game or default" : "Values set by user";
    }
    return true;
}

/**
 * @brief Draws the current setup label.
 *
 * Automatic and Standard custom setups use the two-character labels published by the reference.
 * Advanced slots retain the setup prefix and their zero-based slot number.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] bank Current tuning profile bank.
 */
static void draw_setup_label(DisplayFramebuffer framebuffer, const TuningProfileBank *bank) {
    char label[3] = {'S', (char)('0' + bank->selected_slot), '\0'};
    if (bank->selected_slot == 0) {
        label[0] = 'A';
        label[1] = 'S';
    } else if (bank->standard_mode_enabled && bank->selected_slot == 1) {
        label[0] = 'C';
        label[1] = 'S';
    }
    display_text_draw_with_font(framebuffer, &display_font_10_00c988, label, PAGE_SETUP_LABEL_X,
                                PAGE_SETUP_LABEL_Y, true);
}

/**
 * @brief Renders one local tuning page into the base OLED framebuffer.
 *
 * Reproduces the reference records at 0x01f028: the entry abbreviation is inverted at (13,13),
 * the Font21 primary field begins at (30,30), the optional help line begins at (31,48), and the
 * borderless progress record occupies (30,47)-(225,62).
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] menu Current local tuning selection and visible side.
 * @param[in] bank Current tuning profile bank.
 * @return True when the local tuning page owns the display.
 */
bool display_tuning_page_render(DisplayFramebuffer framebuffer, const TuningMenu *menu,
                                const TuningProfileBank *bank) {
    static TuningPageContent content;
    if (framebuffer == NULL || !display_tuning_page_present(menu, bank, &content)) {
        return false;
    }

    const TuningProfile *profile = tuning_profile_bank_selected(bank);
    display_framebuffer_clear(framebuffer);
    draw_navigation_guide(framebuffer);
    const bool value_view = menu->view == TUNING_MENU_VIEW_VALUE;
    if (menu->selected_entry == TUNING_ENTRY_SETUP) {
        draw_setup_label(framebuffer, bank);
    }
    display_text_draw_with_font(framebuffer, &display_font_10_00c988, content.label, PAGE_ENTRY_X,
                                PAGE_ENTRY_Y, true);
    const char *primary = value_view ? content.value : content.title;
    display_text_draw_with_font(framebuffer, &display_font_21_00aba6, primary, PAGE_PRIMARY_X,
                                PAGE_PRIMARY_Y, false);
    if (value_view) {
        if (menu->selected_entry == TUNING_ENTRY_SETUP) {
            display_text_draw_with_font(framebuffer, &display_font_10_00c988, content.description,
                                        PAGE_HELP_X, PAGE_HELP_Y, false);
        }
    } else {
        display_text_draw_with_font(framebuffer, &display_font_10_00c988, content.description,
                                    PAGE_HELP_X, PAGE_HELP_Y, false);
    }
    if (!tuning_entry_hides_progress(menu->selected_entry, menu->view)) {
        draw_progress(framebuffer, progress_value(menu->selected_entry, profile));
    }
    draw_navigation_spans(framebuffer);
    if (value_view) {
        draw_value_help(framebuffer, menu->selected_entry, profile);
    }
    return true;
}

/**
 * @brief Renders a completed local V3 pedal operation.
 *
 * Selects the short operation result associated with the active pedal phase and places it in the
 * Font21 primary record without changing tuning selection.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] phase Active pedal operation phase.
 * @return True when the phase selected a pedal result.
 */
bool display_tuning_operation_render(DisplayFramebuffer framebuffer, TuningInteractionPhase phase) {
    const char *text;
    if (phase == TUNING_INTERACTION_PEDAL_UP) {
        text = "PEDAL UP";
    } else if (phase == TUNING_INTERACTION_PEDAL_DOWN) {
        text = "PEDAL DOWN";
    } else if (phase == TUNING_INTERACTION_PEDAL_AUTOMATIC) {
        text = "AUTOMATIC";
    } else {
        return false;
    }
    display_framebuffer_clear(framebuffer);
    display_text_draw_with_font(framebuffer, &display_font_21_00aba6, text, PAGE_PRIMARY_X,
                                PAGE_PRIMARY_Y, false);
    return true;
}
