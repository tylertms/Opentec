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
 * @brief Defines tuning-page colors, positions, and description width.
 *
 * These constants control the fixed layout used to render the local OLED tuning page.
 */
enum {
    PAGE_COLOR = 15,                   /**< Foreground grayscale value for tuning-page content. */
    PAGE_LABEL_Y = 0,                  /**< Entry-label top coordinate. */
    PAGE_PRIMARY_Y = 15,               /**< Primary title or value top coordinate. */
    PAGE_DESCRIPTION_Y = 43,           /**< First description-line top coordinate. */
    PAGE_SECOND_DESCRIPTION_Y = 54,    /**< Second description-line top coordinate. */
    PAGE_DESCRIPTION_LINE_LENGTH = 42, /**< Maximum characters kept on one description line. */
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
    [TUNING_ENTRY_VIBRATION_STRENGTH] = {"SHO", "Vibration", "Vibration Motor strength"},
    [TUNING_ENTRY_BRAKE_INDICATOR_LEVEL] = {"BLI", "Brake Level Indicator",
                                            "Required brake input % to trigger vibration"},
    [TUNING_ENTRY_FORCE_SCALE] = {"FFS", "FF Scale", "Force feedback scaling mode"},
    [TUNING_ENTRY_STEERING_DEADZONE] = {"DEA", "Deadzone", "Steering angle deadzone"},
    [TUNING_ENTRY_DRIFT_COMPENSATION] = {"DRI", "Drift Mode", "Wheel axis damping or acceleration"},
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
    [TUNING_ENTRY_BUTTON_ILLUMINATION] = {"BIL", "Button Illumination",
                                          "Toggle button illumination"},
    [TUNING_ENTRY_DISPLAY_ROTATION] = {"DIR", "Display Rotation", "Toggle display rotation"},
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
 * @brief Appends display text to a bounded value field.
 *
 * Preserves termination when the combined text exceeds the available field.
 *
 * @param[in,out] destination Value field receiving appended text.
 * @param[in] size Available destination bytes.
 * @param[in] suffix Null-terminated text to append.
 */
static void append_text(char *destination, size_t size, const char *suffix) {
    size_t length = 0;
    while (length < size && destination[length] != '\0') {
        length++;
    }
    if (length < size) {
        copy_text(destination + length, size - length, suffix);
    }
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
 * @brief Formats a tuning number followed by a unit.
 *
 * Combines the decimal value and its directly visible suffix in one bounded field.
 *
 * @param[out] destination Value field receiving the result.
 * @param[in] size Available destination bytes.
 * @param[in] value Unsigned value to format.
 * @param[in] suffix Unit text appended after the number.
 */
static void format_with_suffix(char *destination, size_t size, uint16_t value, const char *suffix) {
    format_unsigned(destination, size, value);
    append_text(destination, size, suffix);
}

/**
 * @brief Formats a tuning level with its disabled state.
 *
 * Shows `Off` for zero and a scaled decimal value for an enabled level.
 *
 * @param[out] destination Value field receiving the result.
 * @param[in] size Available destination bytes.
 * @param[in] value Stored tuning level.
 * @param[in] scale Visible multiplier for an enabled level.
 */
static void format_level(char *destination, size_t size, uint8_t value, uint8_t scale) {
    if (value == 0) {
        copy_text(destination, size, "Off");
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
    static const char *const multi_position_modes[] = {"Encoder", "Pulse", "Constant", "Auto"};
    /** @brief Display names for analogue paddle modes. */
    static const char *const paddle_modes[] = {"Off", "Clutch Bite Point", "Clutch + Handbrake",
                                               "Brake + Throttle", "Analogue Axes"};
    /** @brief Display names for pedal response curves. */
    static const char *const pedal_curves[] = {"Custom 1", "Custom 2",    "Custom 3",
                                               "Linear",   "Progressive", "Degressive"};

    switch (entry) {
    case TUNING_ENTRY_SETUP:
        format_setup(destination, size, bank);
        break;
    case TUNING_ENTRY_SENSITIVITY:
        if (profile->automatic_rotation != 0) {
            copy_text(destination, size, "Automatic");
        } else {
            format_with_suffix(destination, size, profile->rotation_degrees, " deg");
        }
        break;
    case TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH:
        if (profile->force_feedback_strength == 101) {
            copy_text(destination, size, "Automatic");
        } else {
            format_level(destination, size, profile->force_feedback_strength, 1);
        }
        break;
    case TUNING_ENTRY_VIBRATION_STRENGTH:
        copy_text(destination, size, profile->vibration_strength == 0 ? "Off" : "On");
        break;
    case TUNING_ENTRY_BRAKE_INDICATOR_LEVEL:
        if (profile->brake_indicator_level > 100) {
            copy_text(destination, size, "Off");
        } else {
            format_with_suffix(destination, size, profile->brake_indicator_level, " %");
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
        copy_text(destination, size, profile->drift_compensation == 0 ? "Off" : "On");
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
            format_with_suffix(destination, size, force, " %");
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
        copy_text(
            destination, size,
            paddle_modes[profile->paddle_mode <= TUNING_DUAL_ANALOG ? profile->paddle_mode : 0]);
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
 * @brief Draws a tuning description on one or two centered lines.
 *
 * Uses one centered row when the description fits and otherwise splits at the last available
 * space before the display row limit.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] description Null-terminated description to draw.
 */
static void draw_description(DisplayFramebuffer framebuffer, const char *description) {
    size_t length = 0;
    while (description[length] != '\0') {
        length++;
    }
    if (length <= PAGE_DESCRIPTION_LINE_LENGTH) {
        display_text_draw_centered(framebuffer, description, PAGE_SECOND_DESCRIPTION_Y, 1,
                                   PAGE_COLOR);
        return;
    }

    size_t split = PAGE_DESCRIPTION_LINE_LENGTH;
    while (split != 0 && description[split] != ' ') {
        split--;
    }
    if (split == 0) {
        split = PAGE_DESCRIPTION_LINE_LENGTH;
    }

    char first[PAGE_DESCRIPTION_LINE_LENGTH + 1];
    char second[PAGE_DESCRIPTION_LINE_LENGTH + 1];
    for (size_t index = 0; index < split; index++) {
        first[index] = description[index];
    }
    first[split] = '\0';
    copy_text(second, sizeof(second), description + split + (description[split] == ' ' ? 1 : 0));
    display_text_draw_centered(framebuffer, first, PAGE_DESCRIPTION_Y, 1, PAGE_COLOR);
    display_text_draw_centered(framebuffer, second, PAGE_SECOND_DESCRIPTION_Y, 1, PAGE_COLOR);
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
 * @brief Renders one local tuning page into the base OLED framebuffer.
 *
 * Shows the entry abbreviation, then its title or current value according to the interaction view,
 * followed by the catalog description on one or two centered rows.
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

    display_framebuffer_clear(framebuffer);
    display_text_draw_centered(framebuffer, content.label, PAGE_LABEL_Y, 1, PAGE_COLOR);
    const char *primary = menu->view == TUNING_MENU_VIEW_LABEL ? content.title : content.value;
    uint8_t scale = display_text_width(primary, 2) <= DISPLAY_FRAMEBUFFER_WIDTH ? 2 : 1;
    display_text_draw_centered(framebuffer, primary, PAGE_PRIMARY_Y, scale, PAGE_COLOR);
    draw_description(framebuffer, content.description);
    return true;
}

/**
 * @brief Renders a completed local V3 pedal operation.
 *
 * Selects the short operation result associated with the active pedal phase and centers it on the
 * base display without changing tuning selection.
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
    display_text_draw_centered(framebuffer, text, PAGE_PRIMARY_Y, 2, PAGE_COLOR);
    return true;
}
