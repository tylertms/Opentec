#include "profile/tuning_entry.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "profile/bank.h"
#include "profile/tuning.h"
#include "profile/tuning_interaction.h"

/** @brief Internal tuning-entry encoding and hardware constants. */
enum {
    AUTOMATIC_SENSITIVITY = 126,     /**< Encoded automatic steering selection. */
    SENSITIVITY_ENCODING_BIAS = 127, /**< Bias between encoded sensitivity and degrees. */
    SENSITIVITY_UNIT_DEGREES = 10,   /**< Degrees represented by one sensitivity unit. */
    LEGACY_WHEEL_MODE = 0x0e,        /**< Legacy wheel mode identifier. */
    XBOX_INTERFACE_MODE = 6,         /**< Xbox host interface identifier. */
};

/** @brief Local tuning entries in their display navigation order. */
static const TuningEntry display_order[TUNING_ENTRY_COUNT] = {
    TUNING_ENTRY_SETUP,
    TUNING_ENTRY_SENSITIVITY,
    TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH,
    TUNING_ENTRY_VIBRATION_STRENGTH,
    TUNING_ENTRY_BRAKE_INDICATOR_LEVEL,
    TUNING_ENTRY_FORCE_SCALE,
    TUNING_ENTRY_STEERING_DEADZONE,
    TUNING_ENTRY_DRIFT_COMPENSATION,
    TUNING_ENTRY_FORCE_EFFECT_STRENGTH,
    TUNING_ENTRY_SPRING_EFFECT_STRENGTH,
    TUNING_ENTRY_DAMPER_EFFECT_STRENGTH,
    TUNING_ENTRY_NATURAL_DAMPER,
    TUNING_ENTRY_NATURAL_FRICTION,
    TUNING_ENTRY_BRAKE_FORCE,
    TUNING_ENTRY_ALTERNATE_BRAKE_FORCE,
    TUNING_ENTRY_FORCE_EFFECT_INTENSITY,
    TUNING_ENTRY_MULTI_POSITION_MODE,
    TUNING_ENTRY_PADDLE_MODE,
    TUNING_ENTRY_INTERPOLATION_FILTER,
    TUNING_ENTRY_NATURAL_INERTIA,
    TUNING_ENTRY_FULL_FORCE,
    TUNING_ENTRY_BUTTON_ILLUMINATION,
    TUNING_ENTRY_DISPLAY_ROTATION,
    TUNING_ENTRY_BRAKE_PEDAL_CURVE,
    TUNING_ENTRY_CLUTCH_PEDAL_CURVE,
    TUNING_ENTRY_THROTTLE_PEDAL_CURVE,
};

/** @brief Default adjustment limits for each local tuning entry. */
static const TuningEntryLimits base_limits[TUNING_ENTRY_COUNT] = {
    [TUNING_ENTRY_SETUP] = {1, 6, 1},
    [TUNING_ENTRY_SENSITIVITY] = {-118, 126, 1},
    [TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH] = {0, 100, 1},
    [TUNING_ENTRY_VIBRATION_STRENGTH] = {0, 10, 10},
    [TUNING_ENTRY_BRAKE_INDICATOR_LEVEL] = {1, 101, 1},
    [TUNING_ENTRY_FORCE_SCALE] = {0, 1, 1},
    [TUNING_ENTRY_STEERING_DEADZONE] = {0, 10, 1},
    [TUNING_ENTRY_DRIFT_COMPENSATION] = {0, 1, 1},
    [TUNING_ENTRY_FORCE_EFFECT_STRENGTH] = {0, 12, 1},
    [TUNING_ENTRY_SPRING_EFFECT_STRENGTH] = {0, 12, 1},
    [TUNING_ENTRY_DAMPER_EFFECT_STRENGTH] = {0, 12, 1},
    [TUNING_ENTRY_NATURAL_DAMPER] = {0, 100, 1},
    [TUNING_ENTRY_NATURAL_FRICTION] = {0, 100, 1},
    [TUNING_ENTRY_BRAKE_FORCE] = {0, 100, 1},
    [TUNING_ENTRY_ALTERNATE_BRAKE_FORCE] = {0, 100, 5},
    [TUNING_ENTRY_FORCE_EFFECT_INTENSITY] = {0, 100, 10},
    [TUNING_ENTRY_MULTI_POSITION_MODE] = {0, 3, 1},
    [TUNING_ENTRY_PADDLE_MODE] = {1, 4, 1},
    [TUNING_ENTRY_INTERPOLATION_FILTER] = {0, 20, 1},
    [TUNING_ENTRY_NATURAL_INERTIA] = {0, 100, 1},
    [TUNING_ENTRY_FULL_FORCE] = {0, 1, 1},
    [TUNING_ENTRY_BUTTON_ILLUMINATION] = {0, 1, 1},
    [TUNING_ENTRY_DISPLAY_ROTATION] = {0, 1, 1},
    [TUNING_ENTRY_BRAKE_PEDAL_CURVE] = {0, 5, 1},
    [TUNING_ENTRY_CLUTCH_PEDAL_CURVE] = {0, 5, 1},
    [TUNING_ENTRY_THROTTLE_PEDAL_CURVE] = {0, 5, 1},
};

/**
 * @brief Constrains a signed tuning value to an inclusive interval.
 *
 * Returns the closest endpoint when the requested value is outside the entry limits.
 *
 * @param[in] value Requested tuning value.
 * @param[in] minimum Lowest permitted value.
 * @param[in] maximum Highest permitted value.
 * @return minimum when value is below it, maximum when value is above it, or value otherwise.
 */
static int16_t clamp_value(int16_t value, int16_t minimum, int16_t maximum) {
    if (value < minimum) {
        return minimum;
    }
    return value > maximum ? maximum : value;
}

/**
 * @brief Converts a navigation event to a signed adjustment count.
 *
 * Digital navigation produces one positive or negative count. Analog navigation preserves its
 * signed scale. Other navigation actions do not adjust values.
 *
 * @param[in] navigation Decoded tuning navigation event.
 * @return Signed adjustment count, or zero for a non-adjustment event.
 */
static int16_t adjustment_count(TuningNavigationEvent navigation) {
    if (navigation.mode == TUNING_NAVIGATION_INCREASE) {
        return 1;
    }
    if (navigation.mode == TUNING_NAVIGATION_DECREASE) {
        return -1;
    }
    if (navigation.mode == TUNING_NAVIGATION_ANALOG && navigation.scale != 0) {
        return navigation.scale;
    }
    return 0;
}

TuningEntryLimits tuning_entry_limits(TuningEntry entry, const TuningProfileBank *bank,
                                      const TuningEntryAdjustmentContext *context) {
    if (entry >= TUNING_ENTRY_COUNT || bank == NULL || context == NULL) {
        return (TuningEntryLimits){0};
    }
    TuningEntryLimits limits = base_limits[entry];
    limits.valid = true;
    if (entry == TUNING_ENTRY_ALTERNATE_BRAKE_FORCE && !context->alternate_brake_fine_step) {
        limits.step = 10;
    }
    if (entry == TUNING_ENTRY_MULTI_POSITION_MODE && !context->multi_position_automatic_available) {
        limits.minimum = 1;
    }
    if (!bank->standard_mode_enabled) {
        return limits;
    }
    if (entry == TUNING_ENTRY_SETUP) {
        limits.maximum = 2;
    } else if (entry == TUNING_ENTRY_SENSITIVITY) {
        limits.minimum = -109;
        limits.maximum = -19;
    } else if (entry == TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH) {
        limits.minimum = 5;
    } else if (entry == TUNING_ENTRY_NATURAL_DAMPER) {
        limits.minimum = 25;
    }
    return limits;
}

bool tuning_entry_adjustable_in_automatic_setup(TuningEntry entry) {
    switch (entry) {
    case TUNING_ENTRY_VIBRATION_STRENGTH:
    case TUNING_ENTRY_BRAKE_INDICATOR_LEVEL:
    case TUNING_ENTRY_FORCE_SCALE:
    case TUNING_ENTRY_BRAKE_FORCE:
    case TUNING_ENTRY_ALTERNATE_BRAKE_FORCE:
    case TUNING_ENTRY_MULTI_POSITION_MODE:
    case TUNING_ENTRY_PADDLE_MODE:
    case TUNING_ENTRY_BUTTON_ILLUMINATION:
    case TUNING_ENTRY_DISPLAY_ROTATION:
    case TUNING_ENTRY_BRAKE_PEDAL_CURVE:
    case TUNING_ENTRY_CLUTCH_PEDAL_CURVE:
    case TUNING_ENTRY_THROTTLE_PEDAL_CURVE:
        return true;
    default:
        return false;
    }
}

/**
 * @brief Reports whether an entry supports the active host interface.
 *
 * Deadzone and drift entries are not exposed locally. Force, spring, and damper effect-strength
 * entries are also omitted in Xbox interface mode. The other entries support interface modes zero
 * through eight.
 *
 * @param[in] entry Logical tuning entry.
 * @param[in] interface_mode Active host interface mode.
 * @return true when the entry supports the interface; false otherwise.
 */
static bool entry_supports_interface(TuningEntry entry, uint8_t interface_mode) {
    if (interface_mode > 8 || entry == TUNING_ENTRY_STEERING_DEADZONE ||
        entry == TUNING_ENTRY_DRIFT_COMPENSATION) {
        return false;
    }
    return interface_mode != XBOX_INTERFACE_MODE || (entry != TUNING_ENTRY_FORCE_EFFECT_STRENGTH &&
                                                     entry != TUNING_ENTRY_SPRING_EFFECT_STRENGTH &&
                                                     entry != TUNING_ENTRY_DAMPER_EFFECT_STRENGTH);
}

/**
 * @brief Reports whether attached hardware exposes an entry.
 *
 * Applies wheel capability, pedal connection, pedal calibration, and legacy
 * compatibility gates to entries whose presence depends on attached hardware.
 *
 * @param[in] entry Logical tuning entry.
 * @param[in] bank Current profile bank and tuning mode.
 * @param[in] context Current wheel and pedal capabilities.
 * @return true when attached hardware exposes the entry; false otherwise.
 */
static bool entry_supported_by_hardware(TuningEntry entry, const TuningProfileBank *bank,
                                        const TuningEntryAvailabilityContext *context) {
    switch (entry) {
    case TUNING_ENTRY_FORCE_SCALE:
        return !context->motor_calibration_active;
    case TUNING_ENTRY_NATURAL_DAMPER:
        return context->wheel_auxiliary_state != 0;
    case TUNING_ENTRY_FULL_FORCE:
        return false;
    case TUNING_ENTRY_BUTTON_ILLUMINATION:
    case TUNING_ENTRY_DISPLAY_ROTATION:
        return context->wheel_mode == LEGACY_WHEEL_MODE;
    case TUNING_ENTRY_NATURAL_FRICTION:
    case TUNING_ENTRY_NATURAL_INERTIA:
    case TUNING_ENTRY_INTERPOLATION_FILTER:
        return context->wheel_auxiliary_state == 3;
    case TUNING_ENTRY_BRAKE_FORCE:
        return context->pedal_connection == TUNING_PEDALS_TRANSFER ||
               context->pedal_connection == TUNING_PEDALS_LEGACY;
    case TUNING_ENTRY_BRAKE_PEDAL_CURVE:
    case TUNING_ENTRY_CLUTCH_PEDAL_CURVE:
    case TUNING_ENTRY_THROTTLE_PEDAL_CURVE:
        return context->pedal_connection == TUNING_PEDALS_TRANSFER && !bank->standard_mode_enabled;
    case TUNING_ENTRY_ALTERNATE_BRAKE_FORCE:
        return context->primary_pedal_calibration_active ||
               context->secondary_pedal_calibration_active;
    case TUNING_ENTRY_MULTI_POSITION_MODE:
        return context->multi_position_supported;
    case TUNING_ENTRY_PADDLE_MODE:
        return context->wheel_axis_report_enabled;
    case TUNING_ENTRY_VIBRATION_STRENGTH:
        return context->vibration_mode_compatible;
    case TUNING_ENTRY_BRAKE_INDICATOR_LEVEL:
        return context->legacy_pedal_mode || context->wheel_mode == 0x03 ||
               context->wheel_mode == 0x0a || context->wheel_mode == 0x01 ||
               context->wheel_mode == 0x02 || context->wheel_mode == 0x16;
    default:
        return true;
    }
}

/**
 * @brief Reports whether Standard mode retains an entry.
 *
 * The first two Standard setups retain their setup, steering, force-feedback, vibration, braking,
 * switch, paddle, illumination, display, and pedal-curve controls.
 *
 * @param[in] entry Logical tuning entry.
 * @return true when the Standard setup exposes the entry; false otherwise.
 */
static bool entry_available_in_standard_setup(TuningEntry entry) {
    switch (entry) {
    case TUNING_ENTRY_SETUP:
    case TUNING_ENTRY_SENSITIVITY:
    case TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH:
    case TUNING_ENTRY_VIBRATION_STRENGTH:
    case TUNING_ENTRY_NATURAL_DAMPER:
    case TUNING_ENTRY_BRAKE_FORCE:
    case TUNING_ENTRY_ALTERNATE_BRAKE_FORCE:
    case TUNING_ENTRY_MULTI_POSITION_MODE:
    case TUNING_ENTRY_PADDLE_MODE:
    case TUNING_ENTRY_BUTTON_ILLUMINATION:
    case TUNING_ENTRY_DISPLAY_ROTATION:
    case TUNING_ENTRY_BRAKE_PEDAL_CURVE:
    case TUNING_ENTRY_CLUTCH_PEDAL_CURVE:
    case TUNING_ENTRY_THROTTLE_PEDAL_CURVE:
        return true;
    default:
        return false;
    }
}

bool tuning_entry_available(TuningEntry entry, const TuningProfileBank *bank,
                            const TuningEntryAvailabilityContext *context) {
    if (entry >= TUNING_ENTRY_COUNT || bank == NULL || context == NULL ||
        !entry_supports_interface(entry, context->interface_mode) ||
        !entry_supported_by_hardware(entry, bank, context)) {
        return false;
    }
    return !bank->standard_mode_enabled || bank->active_slot > 1 ||
           entry_available_in_standard_setup(entry);
}

TuningEntry tuning_entry_navigate(TuningEntry current, TuningNavigationMode direction,
                                  const TuningProfileBank *bank,
                                  const TuningEntryAvailabilityContext *context) {
    if ((direction != TUNING_NAVIGATION_PREVIOUS && direction != TUNING_NAVIGATION_NEXT) ||
        bank == NULL || context == NULL) {
        return current;
    }

    uint8_t index = direction == TUNING_NAVIGATION_NEXT ? TUNING_ENTRY_COUNT - 1 : 0;
    for (uint8_t candidate = 0; candidate < TUNING_ENTRY_COUNT; candidate++) {
        if (current == display_order[candidate]) {
            index = candidate;
            break;
        }
    }
    for (uint8_t attempt = 0; attempt < TUNING_ENTRY_COUNT; attempt++) {
        index = direction == TUNING_NAVIGATION_NEXT
                    ? (uint8_t)((index + 1) % TUNING_ENTRY_COUNT)
                    : (uint8_t)((index + TUNING_ENTRY_COUNT - 1) % TUNING_ENTRY_COUNT);
        if (tuning_entry_available(display_order[index], bank, context)) {
            return display_order[index];
        }
    }
    return current;
}

/**
 * @brief Reads a scalar value from a logical tuning profile.
 *
 * Maps entry identifiers to the clean profile representation without relying on layout or pointer
 * tables.
 *
 * @param[in] profile Tuning profile to read.
 * @param[in] entry Logical scalar entry.
 * @return Current entry value, or INT16_MIN when the entry is not scalar.
 */
static int16_t read_profile_value(const TuningProfile *profile, TuningEntry entry) {
    switch (entry) {
    case TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH:
        return profile->force_feedback_strength;
    case TUNING_ENTRY_VIBRATION_STRENGTH:
        return profile->vibration_strength;
    case TUNING_ENTRY_BRAKE_INDICATOR_LEVEL:
        return profile->brake_indicator_level;
    case TUNING_ENTRY_FORCE_SCALE:
        return profile->force_scale;
    case TUNING_ENTRY_STEERING_DEADZONE:
        return profile->steering_deadzone;
    case TUNING_ENTRY_DRIFT_COMPENSATION:
        return profile->drift_compensation;
    case TUNING_ENTRY_FORCE_EFFECT_STRENGTH:
        return profile->force_effect_strength;
    case TUNING_ENTRY_SPRING_EFFECT_STRENGTH:
        return profile->spring_effect_strength;
    case TUNING_ENTRY_DAMPER_EFFECT_STRENGTH:
        return profile->damper_effect_strength;
    case TUNING_ENTRY_NATURAL_DAMPER:
        return profile->natural_damper;
    case TUNING_ENTRY_NATURAL_FRICTION:
        return profile->natural_friction;
    case TUNING_ENTRY_BRAKE_FORCE:
        return profile->brake_force;
    case TUNING_ENTRY_ALTERNATE_BRAKE_FORCE:
        return profile->alternate_brake_force;
    case TUNING_ENTRY_FORCE_EFFECT_INTENSITY:
        return profile->force_effect_intensity;
    case TUNING_ENTRY_MULTI_POSITION_MODE:
        return profile->multi_position_mode;
    case TUNING_ENTRY_PADDLE_MODE:
        return profile->paddle_mode;
    case TUNING_ENTRY_INTERPOLATION_FILTER:
        return profile->interpolation_filter;
    case TUNING_ENTRY_NATURAL_INERTIA:
        return profile->natural_inertia;
    case TUNING_ENTRY_FULL_FORCE:
        return profile->full_force_enabled;
    case TUNING_ENTRY_BUTTON_ILLUMINATION:
        return profile->button_illumination_enabled;
    case TUNING_ENTRY_DISPLAY_ROTATION:
        return profile->display_rotation_enabled;
    case TUNING_ENTRY_BRAKE_PEDAL_CURVE:
        return profile->brake_pedal_curve;
    case TUNING_ENTRY_CLUTCH_PEDAL_CURVE:
        return profile->clutch_pedal_curve;
    case TUNING_ENTRY_THROTTLE_PEDAL_CURVE:
        return profile->throttle_pedal_curve;
    default:
        return INT16_MIN;
    }
}

/**
 * @brief Writes a scalar value to a logical tuning profile.
 *
 * Maps entry identifiers to their typed profile fields without depending on structure layout.
 *
 * @param[in,out] profile Tuning profile to update.
 * @param[in] entry Logical scalar entry.
 * @param[in] value Constrained entry value.
 * @return true when the entry maps to a scalar profile value; false otherwise.
 */
static bool write_profile_value(TuningProfile *profile, TuningEntry entry, int16_t value) {
    switch (entry) {
    case TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH:
        profile->force_feedback_strength = (uint8_t)value;
        break;
    case TUNING_ENTRY_VIBRATION_STRENGTH:
        profile->vibration_strength = (uint8_t)value;
        break;
    case TUNING_ENTRY_BRAKE_INDICATOR_LEVEL:
        profile->brake_indicator_level = (uint8_t)value;
        break;
    case TUNING_ENTRY_FORCE_SCALE:
        profile->force_scale = (TuningForceScale)value;
        break;
    case TUNING_ENTRY_STEERING_DEADZONE:
        profile->steering_deadzone = (uint8_t)value;
        break;
    case TUNING_ENTRY_DRIFT_COMPENSATION:
        profile->drift_compensation = (uint8_t)value;
        break;
    case TUNING_ENTRY_FORCE_EFFECT_STRENGTH:
        profile->force_effect_strength = (uint8_t)value;
        break;
    case TUNING_ENTRY_SPRING_EFFECT_STRENGTH:
        profile->spring_effect_strength = (uint8_t)value;
        break;
    case TUNING_ENTRY_DAMPER_EFFECT_STRENGTH:
        profile->damper_effect_strength = (uint8_t)value;
        break;
    case TUNING_ENTRY_NATURAL_DAMPER:
        profile->natural_damper = (uint8_t)value;
        break;
    case TUNING_ENTRY_NATURAL_FRICTION:
        profile->natural_friction = (uint8_t)value;
        break;
    case TUNING_ENTRY_BRAKE_FORCE:
        profile->brake_force = (uint8_t)value;
        break;
    case TUNING_ENTRY_ALTERNATE_BRAKE_FORCE:
        profile->alternate_brake_force = (uint8_t)value;
        break;
    case TUNING_ENTRY_FORCE_EFFECT_INTENSITY:
        profile->force_effect_intensity = (uint8_t)value;
        break;
    case TUNING_ENTRY_MULTI_POSITION_MODE:
        profile->multi_position_mode = (TuningMultiPositionMode)value;
        break;
    case TUNING_ENTRY_PADDLE_MODE:
        profile->paddle_mode = (TuningPaddleMode)value;
        break;
    case TUNING_ENTRY_INTERPOLATION_FILTER:
        profile->interpolation_filter = (uint8_t)value;
        break;
    case TUNING_ENTRY_NATURAL_INERTIA:
        profile->natural_inertia = (uint8_t)value;
        break;
    case TUNING_ENTRY_FULL_FORCE:
        profile->full_force_enabled = (uint8_t)value;
        break;
    case TUNING_ENTRY_BUTTON_ILLUMINATION:
        profile->button_illumination_enabled = (uint8_t)value;
        break;
    case TUNING_ENTRY_DISPLAY_ROTATION:
        profile->display_rotation_enabled = (uint8_t)value;
        break;
    case TUNING_ENTRY_BRAKE_PEDAL_CURVE:
        profile->brake_pedal_curve = (TuningPedalCurve)value;
        break;
    case TUNING_ENTRY_CLUTCH_PEDAL_CURVE:
        profile->clutch_pedal_curve = (TuningPedalCurve)value;
        break;
    case TUNING_ENTRY_THROTTLE_PEDAL_CURVE:
        profile->throttle_pedal_curve = (TuningPedalCurve)value;
        break;
    default:
        return false;
    }
    return true;
}

/**
 * @brief Adjusts the selected and active tuning setup.
 *
 * Applies the navigation count to the one-based setup number, constrains it to the active tuning
 * mode, and immediately activates a changed selection.
 *
 * @param[in,out] bank Tuning profile bank to update.
 * @param[in] count Signed adjustment count.
 * @param[in] limits Active setup limits.
 * @return true when the selected setup changed; false when it remains unchanged or invalid.
 */
static bool adjust_setup(TuningProfileBank *bank, int16_t count, TuningEntryLimits limits) {
    uint8_t selected = (uint8_t)(bank->selected_slot + 1);
    uint8_t adjusted = (uint8_t)clamp_value((int16_t)selected + count * limits.step, limits.minimum,
                                            limits.maximum);
    if (adjusted == selected || !tuning_profile_bank_select(bank, (uint8_t)(adjusted - 1))) {
        return false;
    }
    tuning_profile_bank_activate_selected(bank);
    return true;
}

/**
 * @brief Adjusts the selected profile's steering sensitivity.
 *
 * Uses the signed ten-degree representation to preserve the automatic value immediately above the
 * manual range. A positive overflow selects automatic steering range.
 *
 * @param[in,out] profile Selected tuning profile.
 * @param[in] count Signed adjustment count.
 * @param[in] limits Active sensitivity limits.
 * @return true when automatic selection or the concrete range changed; false when neither changed.
 */
static bool adjust_sensitivity(TuningProfile *profile, int16_t count, TuningEntryLimits limits) {
    int16_t encoded = profile->automatic_rotation != 0
                          ? AUTOMATIC_SENSITIVITY
                          : (int16_t)(profile->rotation_degrees / SENSITIVITY_UNIT_DEGREES) -
                                SENSITIVITY_ENCODING_BIAS;
    uint8_t step =
        ((encoded >= -18 && encoded <= 125) || (encoded == -19 && count > 0)) ? 9 : limits.step;
    int16_t requested = encoded + count * step;
    int16_t adjusted = requested > limits.maximum && count > 0
                           ? AUTOMATIC_SENSITIVITY
                           : clamp_value(requested, limits.minimum, limits.maximum);
    bool automatic = adjusted == AUTOMATIC_SENSITIVITY;
    uint16_t degrees =
        automatic ? profile->rotation_degrees
                  : (uint16_t)(adjusted + SENSITIVITY_ENCODING_BIAS) * SENSITIVITY_UNIT_DEGREES;
    if (profile->automatic_rotation == automatic && profile->rotation_degrees == degrees) {
        return false;
    }
    profile->automatic_rotation = automatic;
    profile->rotation_degrees = degrees;
    return true;
}

bool tuning_entry_adjust(TuningProfileBank *bank, TuningEntry entry,
                         TuningNavigationEvent navigation,
                         const TuningEntryAdjustmentContext *context) {
    int16_t count = adjustment_count(navigation);
    TuningEntryLimits limits = tuning_entry_limits(entry, bank, context);
    if (count == 0 || !limits.valid || context->security_code_active ||
        (context->automatic_setup_selected && entry != TUNING_ENTRY_SETUP &&
         !tuning_entry_adjustable_in_automatic_setup(entry))) {
        return false;
    }
    if (entry == TUNING_ENTRY_SETUP) {
        return adjust_setup(bank, count, limits);
    }

    TuningProfile *profile = &bank->slots[bank->selected_slot];
    if (entry == TUNING_ENTRY_SENSITIVITY) {
        return adjust_sensitivity(profile, count, limits);
    }

    int16_t current = read_profile_value(profile, entry);
    if (current == INT16_MIN) {
        return false;
    }
    int16_t adjusted = clamp_value(current + count * limits.step, limits.minimum, limits.maximum);
    return adjusted != current && write_profile_value(profile, entry, adjusted);
}
