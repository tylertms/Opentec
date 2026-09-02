#include "usb/fallback_tuning.h"

#include <stdbool.h>
#include <stdint.h>

#include "wheel/position.h"

/** @brief Native physical steering-range conversion constants. */
enum {
    FALLBACK_LOW_ROTATION_DEGREES = 200,
    FALLBACK_STEERING_LIMIT_MINIMUM_INPUT = 89,
    FALLBACK_STEERING_LIMIT_MAXIMUM_UNITS =
        TUNING_ROTATION_MAX_DEGREES / TUNING_ROTATION_STEP_DEGREES,
};

bool usb_fallback_tuning_range_allowed(const TuningProfile *profile) {
    return profile != NULL &&
           (profile->automatic_rotation != 0 || profile->rotation_degrees == 1300);
}

bool usb_fallback_tuning_steering_travel(const UsbFallbackCommand *command, uint32_t *travel) {
    if (command == NULL || travel == NULL) {
        return false;
    }

    switch (command->kind) {
    case USB_FALLBACK_STEERING_RANGE_LOW:
        *travel = wheel_position_travel_from_degrees(FALLBACK_LOW_ROTATION_DEGREES);
        return true;
    case USB_FALLBACK_STEERING_RANGE_HIGH:
        *travel = WHEEL_POSITION_SAMPLE_LIMIT;
        return true;
    case USB_FALLBACK_STEERING_LIMIT: {
        if (command->value <= FALLBACK_STEERING_LIMIT_MINIMUM_INPUT) {
            *travel = wheel_position_travel_from_degrees(TUNING_ROTATION_MIN_DEGREES);
            return true;
        }
        uint32_t travel_limit = (uint32_t)(command->value / TUNING_ROTATION_STEP_DEGREES) *
                                WHEEL_POSITION_SAMPLE_LIMIT / FALLBACK_STEERING_LIMIT_MAXIMUM_UNITS;
        *travel =
            travel_limit > WHEEL_POSITION_SAMPLE_LIMIT ? WHEEL_POSITION_SAMPLE_LIMIT : travel_limit;
        return true;
    }
    default:
        return false;
    }
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
