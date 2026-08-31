#include "usb/fallback_tuning.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Reports whether fallback steering-range commands may replace the active range.
 *
 * Accepts the official manual 1300-degree sensitivity and automatic sensitivity selections.
 *
 * @param[in] profile Active runtime tuning profile.
 * @return True when a fallback range command may be applied.
 */
bool usb_fallback_tuning_range_allowed(const TuningProfile *profile) {
    return profile != NULL &&
           (profile->automatic_rotation != 0 || profile->rotation_degrees == 1300);
}

/**
 * @brief Constrains an official fallback setting to its supported maximum.
 *
 * Preserves values within the inclusive range from zero through the supplied maximum.
 *
 * @param[in] value Requested unsigned setting.
 * @param[in] maximum Highest accepted setting.
 * @return Requested value or the supported maximum.
 */
static uint8_t clamp_setting(uint8_t value, uint8_t maximum) {
    return value > maximum ? maximum : value;
}

/**
 * @brief Applies one transient setup-one fallback tuning command.
 *
 * Changes only the supplied runtime profile. Values use the official command-specific limits;
 * sensitivity decodes its signed ten-degree representation and force scale accepts only its two
 * defined selectors.
 *
 * @param[in] command Decoded fallback tuning command.
 * @param[in] active_slot Zero-based active tuning setup.
 * @param[in,out] profile Runtime tuning profile to update.
 * @return True when the command changed an eligible runtime profile.
 */
bool usb_fallback_tuning_apply(const UsbFallbackCommand *command, uint8_t active_slot,
                               TuningProfile *profile) {
    if (command == NULL || profile == NULL || active_slot != 0) {
        return false;
    }

    switch (command->kind) {
    case USB_FALLBACK_SENSITIVITY: {
        uint8_t encoded = (uint8_t)(command->value / 10U - 127U);
        if (encoded == 126U) {
            profile->automatic_rotation = 1;
            profile->rotation_degrees = TUNING_ROTATION_MAX_DEGREES;
        } else {
            int16_t signed_encoded = encoded <= INT8_MAX ? encoded : (int16_t)encoded - 256;
            int16_t units = signed_encoded + 127;
            profile->automatic_rotation = 0;
            profile->rotation_degrees =
                units > 0 ? (uint16_t)units * TUNING_ROTATION_STEP_DEGREES : 0;
        }
        return true;
    }
    case USB_FALLBACK_FORCE_FEEDBACK_STRENGTH:
        profile->force_feedback_strength = clamp_setting(command->parameters[0], 100);
        return true;
    case USB_FALLBACK_FORCE_SCALE:
        if (command->parameters[0] == 1) {
            profile->force_scale = TUNING_FORCE_SCALE_LINEAR;
            return true;
        }
        if (command->parameters[0] == 2) {
            profile->force_scale = TUNING_FORCE_SCALE_PEAK;
            return true;
        }
        return false;
    case USB_FALLBACK_NATURAL_DAMPER:
        profile->natural_damper = clamp_setting(command->parameters[0], 100);
        return true;
    case USB_FALLBACK_NATURAL_FRICTION:
        profile->natural_friction = clamp_setting(command->parameters[0], 100);
        return true;
    case USB_FALLBACK_NATURAL_INERTIA:
        profile->natural_inertia = clamp_setting(command->parameters[0], 100);
        return true;
    case USB_FALLBACK_INTERPOLATION:
        profile->interpolation_filter = clamp_setting(command->parameters[0], 20);
        return true;
    case USB_FALLBACK_FORCE_EFFECT_INTENSITY:
        profile->force_effect_intensity = clamp_setting(command->parameters[0], 100);
        return true;
    case USB_FALLBACK_FORCE_EFFECT_STRENGTH:
        profile->force_effect_strength = clamp_setting(command->parameters[0], 12);
        return true;
    case USB_FALLBACK_SPRING_EFFECT_STRENGTH:
        profile->spring_effect_strength = clamp_setting(command->parameters[0], 12);
        return true;
    case USB_FALLBACK_DAMPER_EFFECT_STRENGTH:
        profile->damper_effect_strength = clamp_setting(command->parameters[0], 12);
        return true;
    case USB_FALLBACK_VIBRATION_STRENGTH:
        profile->vibration_strength = clamp_setting(command->parameters[0], 12);
        return true;
    default:
        return false;
    }
}
