#include "profile/tuning.h"

#include <stdint.h>

static uint8_t clamp_u8(uint8_t value, uint8_t minimum, uint8_t maximum) {
    if (value < minimum) {
        return minimum;
    }
    return value > maximum ? maximum : value;
}

static uint16_t normalize_rotation(uint16_t degrees) {
    if (degrees < 90) {
        return 90;
    }
    if (degrees > 2520) {
        return 2520;
    }
    return (uint16_t)(degrees / 10 * 10);
}

static uint8_t normalize_boolean(uint8_t value) { return value != 0; }

static TuningForceScale normalize_force_scale(TuningForceScale value) {
    return value == TUNING_FORCE_SCALE_PEAK ? value : TUNING_FORCE_SCALE_LINEAR;
}

static TuningMultiPositionMode normalize_multi_position(TuningMultiPositionMode value) {
    return value <= TUNING_MULTI_POSITION_AUTOMATIC ? value : TUNING_MULTI_POSITION_AUTOMATIC;
}

static TuningPaddleMode normalize_paddle_mode(TuningPaddleMode value) {
    return value >= TUNING_CLUTCH_BRAKE && value <= TUNING_DUAL_ANALOG ? value
                                                                       : TUNING_CLUTCH_BRAKE;
}

static TuningPedalCurve normalize_pedal_curve(TuningPedalCurve value) {
    return value <= TUNING_PEDAL_CURVE_DEGREES ? value : TUNING_PEDAL_CURVE_LINEAR;
}

void tuning_profile_defaults(TuningProfile *profile) {
    *profile = (TuningProfile){
        .rotation_degrees = 1080,
        .automatic_rotation = 1,
        .force_feedback_strength = 35,
        .vibration_strength = 10,
        .brake_indicator_level = 101,
        .force_scale = TUNING_FORCE_SCALE_PEAK,
        .steering_deadzone = 0,
        .drift_compensation = 0,
        .force_effect_strength = 10,
        .spring_effect_strength = 10,
        .damper_effect_strength = 10,
        .natural_damper = 50,
        .natural_friction = 0,
        .brake_force = 50,
        .alternate_brake_force = 50,
        .force_effect_intensity = 100,
        .multi_position_mode = TUNING_MULTI_POSITION_AUTOMATIC,
        .paddle_mode = TUNING_CLUTCH_BRAKE,
        .interpolation_filter = 6,
        .natural_inertia = 0,
        .full_force_enabled = 0,
        .button_illumination_enabled = 1,
        .display_rotation_enabled = 1,
        .brake_pedal_curve = TUNING_PEDAL_CURVE_LINEAR,
        .clutch_pedal_curve = TUNING_PEDAL_CURVE_LINEAR,
        .throttle_pedal_curve = TUNING_PEDAL_CURVE_LINEAR,
    };
}

void tuning_profile_normalize(TuningProfile *profile) {
    profile->rotation_degrees = normalize_rotation(profile->rotation_degrees);
    profile->automatic_rotation = normalize_boolean(profile->automatic_rotation);
    profile->force_feedback_strength = clamp_u8(profile->force_feedback_strength, 0, 100);
    profile->vibration_strength =
        clamp_u8(profile->vibration_strength, 0, TUNING_VIBRATION_STRENGTH_MAX);
    profile->brake_indicator_level = clamp_u8(profile->brake_indicator_level, 1, 101);
    profile->force_scale = normalize_force_scale(profile->force_scale);
    profile->steering_deadzone = clamp_u8(profile->steering_deadzone, 0, 10);
    profile->drift_compensation = normalize_boolean(profile->drift_compensation);
    profile->force_effect_strength = clamp_u8(profile->force_effect_strength, 0, 12);
    profile->spring_effect_strength = clamp_u8(profile->spring_effect_strength, 0, 12);
    profile->damper_effect_strength = clamp_u8(profile->damper_effect_strength, 0, 12);
    profile->natural_damper = clamp_u8(profile->natural_damper, 0, 100);
    profile->natural_friction = clamp_u8(profile->natural_friction, 0, 100);
    profile->brake_force = clamp_u8(profile->brake_force, 0, 100);
    profile->alternate_brake_force = clamp_u8(profile->alternate_brake_force, 0, 100);
    profile->force_effect_intensity = clamp_u8(profile->force_effect_intensity, 0, 100);
    profile->multi_position_mode = normalize_multi_position(profile->multi_position_mode);
    profile->paddle_mode = normalize_paddle_mode(profile->paddle_mode);
    profile->interpolation_filter = clamp_u8(profile->interpolation_filter, 0, 20);
    profile->natural_inertia = clamp_u8(profile->natural_inertia, 0, 100);
    profile->full_force_enabled = normalize_boolean(profile->full_force_enabled);
    profile->button_illumination_enabled = normalize_boolean(profile->button_illumination_enabled);
    profile->display_rotation_enabled = normalize_boolean(profile->display_rotation_enabled);
    profile->brake_pedal_curve = normalize_pedal_curve(profile->brake_pedal_curve);
    profile->clutch_pedal_curve = normalize_pedal_curve(profile->clutch_pedal_curve);
    profile->throttle_pedal_curve = normalize_pedal_curve(profile->throttle_pedal_curve);
}
