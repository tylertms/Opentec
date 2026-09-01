#include "profile/tuning_display.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "profile/bank.h"
#include "profile/tuning.h"
#include "profile/tuning_entry.h"
#include "profile/tuning_menu.h"
#include "wheel/display_output.h"

/** @brief Three-character labels for local tuning entries. */
static const char entry_labels[TUNING_ENTRY_COUNT][4] = {
    [TUNING_ENTRY_SETUP] = "S_ ",
    [TUNING_ENTRY_SENSITIVITY] = "SEN",
    [TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH] = "FFB",
    [TUNING_ENTRY_VIBRATION_STRENGTH] = "SHO",
    [TUNING_ENTRY_BRAKE_INDICATOR_LEVEL] = "BLI",
    [TUNING_ENTRY_FORCE_SCALE] = "FFS",
    [TUNING_ENTRY_STEERING_DEADZONE] = "DEA",
    [TUNING_ENTRY_DRIFT_COMPENSATION] = "DRI",
    [TUNING_ENTRY_FORCE_EFFECT_STRENGTH] = "FOR",
    [TUNING_ENTRY_SPRING_EFFECT_STRENGTH] = "SPR",
    [TUNING_ENTRY_DAMPER_EFFECT_STRENGTH] = "DPR",
    [TUNING_ENTRY_NATURAL_DAMPER] = "NDP",
    [TUNING_ENTRY_NATURAL_FRICTION] = "NFR",
    [TUNING_ENTRY_BRAKE_FORCE] = "BRF",
    [TUNING_ENTRY_ALTERNATE_BRAKE_FORCE] = "BRF",
    [TUNING_ENTRY_FORCE_EFFECT_INTENSITY] = "FEI",
    [TUNING_ENTRY_MULTI_POSITION_MODE] = "MPS",
    [TUNING_ENTRY_PADDLE_MODE] = "ACP",
    [TUNING_ENTRY_INTERPOLATION_FILTER] = "INT",
    [TUNING_ENTRY_NATURAL_INERTIA] = "NIN",
    [TUNING_ENTRY_FULL_FORCE] = "FUL",
    [TUNING_ENTRY_BUTTON_ILLUMINATION] = "BIL",
    [TUNING_ENTRY_DISPLAY_ROTATION] = "DIR",
    [TUNING_ENTRY_BRAKE_PEDAL_CURVE] = "BPC",
    [TUNING_ENTRY_CLUTCH_PEDAL_CURVE] = "CPC",
    [TUNING_ENTRY_THROTTLE_PEDAL_CURVE] = "TPC",
};

/**
 * @brief Encodes one display character as a raw seven-segment glyph.
 *
 * Applies the digit and uppercase-letter maps used by local tuning text, plus the supported dash,
 * underscore, and space characters.
 *
 * @param[in] character Character to encode.
 * @return Mapped glyph when character is supported; zero otherwise.
 */
static uint8_t character_glyph(char character) {
    /** @brief Seven-segment glyphs for decimal digits. */
    static const uint8_t digit_glyphs[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66,
                                           0x6d, 0x7d, 0x07, 0x7f, 0x6f};
    /** @brief Seven-segment glyphs for uppercase letters. */
    static const uint8_t letter_glyphs[] = {
        0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71, 0x7d, 0x76, 0x30, 0x00, 0x00, 0x38, 0x00,
        0x54, 0x5c, 0x73, 0x00, 0x50, 0x6d, 0x78, 0x3e, 0x1c, 0x00, 0x00, 0x7a, 0x00,
    };
    if (character >= '0' && character <= '9') {
        return digit_glyphs[(uint8_t)(character - '0')];
    }
    if (character >= 'A' && character <= 'Z') {
        return letter_glyphs[(uint8_t)(character - 'A')];
    }
    if (character == '-') {
        return 0x40;
    }
    if (character == '_') {
        return 0x08;
    }
    return 0;
}

/**
 * @brief Replaces the three visible tuning glyphs with text.
 *
 * Encodes exactly three characters and clears the separate third-position marker without changing
 * auxiliary wheel output.
 *
 * @param[in,out] output Attached-wheel output receiving the text.
 * @param[in] text Three-character tuning text.
 */
static void render_text(WheelDisplayOutput *output, const char text[4]) {
    for (uint8_t index = 0; index < WHEEL_DISPLAY_GLYPH_COUNT; index++) {
        output->glyphs[index] = character_glyph(text[index]);
    }
    output->third_glyph_marker = false;
}

/**
 * @brief Renders a nonnegative tuning number on three positions.
 *
 * Right-aligns one- and two-digit values and uses all positions for values from one hundred through
 * nine hundred ninety-nine.
 *
 * @param[in,out] output Attached-wheel output receiving the number.
 * @param[in] value Tuning value from zero through nine hundred ninety-nine.
 */
static void render_number(WheelDisplayOutput *output, uint16_t value) {
    output->glyphs[0] = value >= 100 ? character_glyph((char)('0' + value / 100u)) : 0;
    output->glyphs[1] = value >= 10 ? character_glyph((char)('0' + value / 10u % 10u)) : 0;
    output->glyphs[2] = character_glyph((char)('0' + value % 10u));
    output->third_glyph_marker = false;
}

/**
 * @brief Renders an enabled or disabled tuning flag.
 *
 * Shows `ON` for an enabled flag and `OFF` for a disabled flag.
 *
 * @param[in,out] output Attached-wheel output receiving the flag.
 * @param[in] enabled True to show the enabled state.
 */
static void render_boolean(WheelDisplayOutput *output, bool enabled) {
    render_text(output, enabled ? "ON " : "OFF");
}

/**
 * @brief Renders an effect level with its disabled state.
 *
 * Shows `0FF` for zero and otherwise multiplies the stored level by the requested display scale.
 *
 * @param[in,out] output Attached-wheel output receiving the effect level.
 * @param[in] value Stored effect level.
 * @param[in] scale Display multiplier applied to an enabled level.
 */
static void render_effect_level(WheelDisplayOutput *output, uint8_t value, uint8_t scale) {
    if (value == 0) {
        render_text(output, "0FF");
    } else {
        render_number(output, (uint16_t)value * scale);
    }
}

/**
 * @brief Renders the selected setup value.
 *
 * Shows the automatic setup, the Standard-mode custom setup, or the numbered Advanced setup.
 *
 * @param[in,out] output Attached-wheel output receiving the setup value.
 * @param[in] bank Current tuning profile bank.
 */
static void render_setup(WheelDisplayOutput *output, const TuningProfileBank *bank) {
    if (bank->selected_slot == 0) {
        render_text(output, "A_S");
        return;
    }
    if (bank->standard_mode_enabled && bank->selected_slot == 1) {
        render_text(output, "C_S");
        return;
    }
    render_text(output, "S_ ");
    output->glyphs[2] = character_glyph((char)('0' + bank->selected_slot));
}

/**
 * @brief Renders the steering sensitivity value.
 *
 * Shows automatic steering selection or the concrete lock-to-lock range. Four-digit ranges use a
 * trailing marker after their tens-of-degrees representation.
 *
 * @param[in,out] output Attached-wheel output receiving steering sensitivity.
 * @param[in] profile Selected tuning profile.
 */
static void render_sensitivity(WheelDisplayOutput *output, const TuningProfile *profile) {
    if (profile->automatic_rotation != 0) {
        render_text(output, "AUT");
        return;
    }
    if (profile->rotation_degrees < 1000) {
        render_number(output, profile->rotation_degrees);
        return;
    }
    render_number(output, profile->rotation_degrees / 10u);
    output->glyphs[2] |= 0x80u;
}

/**
 * @brief Renders one tuning entry value.
 *
 * Applies the entry-specific number scaling and the setup, automatic, disabled, limit, mode,
 * paddle, and pedal-curve labels used by the local three-character presentation.
 *
 * @param[in,out] output Attached-wheel output receiving the value.
 * @param[in] entry Selected logical tuning entry.
 * @param[in] bank Current tuning profile bank.
 * @param[in] profile Selected tuning profile.
 */
static void render_value(WheelDisplayOutput *output, TuningEntry entry,
                         const TuningProfileBank *bank, const TuningProfile *profile) {
    switch (entry) {
    case TUNING_ENTRY_SETUP:
        render_setup(output, bank);
        break;
    case TUNING_ENTRY_SENSITIVITY:
        render_sensitivity(output, profile);
        break;
    case TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH:
        if (profile->force_feedback_strength == 101) {
            render_text(output, "AUT");
        } else {
            render_effect_level(output, profile->force_feedback_strength, 1);
        }
        break;
    case TUNING_ENTRY_VIBRATION_STRENGTH:
        render_text(output, profile->vibration_strength == 0 ? "0FF" : "0N ");
        break;
    case TUNING_ENTRY_BRAKE_INDICATOR_LEVEL:
        if (profile->brake_indicator_level > 100) {
            render_text(output, "0FF");
        } else {
            render_number(output, profile->brake_indicator_level);
        }
        break;
    case TUNING_ENTRY_FORCE_SCALE:
        render_text(output, profile->force_scale == TUNING_FORCE_SCALE_PEAK ? "PEA" : "LIN");
        break;
    case TUNING_ENTRY_STEERING_DEADZONE:
        render_effect_level(output, profile->steering_deadzone, 10);
        break;
    case TUNING_ENTRY_DRIFT_COMPENSATION:
        render_effect_level(output, profile->drift_compensation, 1);
        break;
    case TUNING_ENTRY_FORCE_EFFECT_STRENGTH:
        render_effect_level(output, profile->force_effect_strength, 10);
        break;
    case TUNING_ENTRY_SPRING_EFFECT_STRENGTH:
        render_effect_level(output, profile->spring_effect_strength, 10);
        break;
    case TUNING_ENTRY_DAMPER_EFFECT_STRENGTH:
        render_effect_level(output, profile->damper_effect_strength, 10);
        break;
    case TUNING_ENTRY_NATURAL_DAMPER:
        render_effect_level(output, profile->natural_damper, 1);
        break;
    case TUNING_ENTRY_NATURAL_FRICTION:
        render_effect_level(output, profile->natural_friction, 1);
        break;
    case TUNING_ENTRY_BRAKE_FORCE:
        if (profile->brake_force == 0) {
            render_text(output, "LO ");
            output->glyphs[1] |= 0x80u;
        } else if (profile->brake_force == 100) {
            render_text(output, "HI ");
            output->glyphs[1] |= 0x80u;
        } else {
            render_number(output, profile->brake_force);
        }
        break;
    case TUNING_ENTRY_ALTERNATE_BRAKE_FORCE:
        if (profile->alternate_brake_force == 0) {
            render_text(output, "LO ");
            output->glyphs[1] |= 0x80u;
        } else if (profile->alternate_brake_force == 100) {
            render_text(output, "HI ");
            output->glyphs[1] |= 0x80u;
        } else {
            render_number(output, profile->alternate_brake_force);
        }
        break;
    case TUNING_ENTRY_FORCE_EFFECT_INTENSITY:
        render_number(output, profile->force_effect_intensity);
        break;
    case TUNING_ENTRY_MULTI_POSITION_MODE:
        render_number(output, profile->multi_position_mode);
        break;
    case TUNING_ENTRY_PADDLE_MODE: {
        /** @brief Display labels for paddle assignment modes. */
        static const char labels[][4] = {"OFF", "CBP", "C H", "B T", "ANA"};
        uint8_t mode =
            profile->paddle_mode <= TUNING_DUAL_ANALOG ? (uint8_t)profile->paddle_mode : 0;
        render_text(output, labels[mode]);
        break;
    }
    case TUNING_ENTRY_INTERPOLATION_FILTER:
        render_effect_level(output, profile->interpolation_filter, 1);
        break;
    case TUNING_ENTRY_NATURAL_INERTIA:
        render_effect_level(output, profile->natural_inertia, 1);
        break;
    case TUNING_ENTRY_FULL_FORCE:
        render_boolean(output, profile->full_force_enabled != 0);
        break;
    case TUNING_ENTRY_BUTTON_ILLUMINATION:
        render_boolean(output, profile->button_illumination_enabled != 0);
        break;
    case TUNING_ENTRY_DISPLAY_ROTATION:
        render_boolean(output, profile->display_rotation_enabled != 0);
        break;
    case TUNING_ENTRY_BRAKE_PEDAL_CURVE:
    case TUNING_ENTRY_CLUTCH_PEDAL_CURVE:
    case TUNING_ENTRY_THROTTLE_PEDAL_CURVE: {
        /** @brief Display labels for pedal response curves. */
        static const char labels[][4] = {"C 1", "C 2", "C 3", "LIN", "PRO", "DEG"};
        TuningPedalCurve curve =
            entry == TUNING_ENTRY_BRAKE_PEDAL_CURVE    ? profile->brake_pedal_curve
            : entry == TUNING_ENTRY_CLUTCH_PEDAL_CURVE ? profile->clutch_pedal_curve
                                                       : profile->throttle_pedal_curve;
        render_text(output, labels[curve <= TUNING_PEDAL_CURVE_DEGREES ? curve : 0]);
        break;
    }
    default:
        render_text(output, "   ");
        break;
    }
}

bool tuning_display_render(const TuningMenu *menu, const TuningProfileBank *bank,
                           WheelDisplayOutput *output) {
    if (menu == NULL || bank == NULL || output == NULL ||
        menu->selected_entry >= TUNING_ENTRY_COUNT) {
        return false;
    }
    const TuningProfile *profile = tuning_profile_bank_selected(bank);
    if (profile == NULL) {
        return false;
    }
    if (menu->view == TUNING_MENU_VIEW_LABEL) {
        render_text(output, entry_labels[menu->selected_entry]);
    } else {
        render_value(output, menu->selected_entry, bank, profile);
    }
    return true;
}
