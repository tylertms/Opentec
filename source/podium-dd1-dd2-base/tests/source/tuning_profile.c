#include <assert.h>
#include <stdint.h>

#include "profile/tuning.h"

static void test_defaults(void) {
    TuningProfile profile;
    tuning_profile_defaults(&profile);

    assert(profile.rotation_degrees == 1080);
    assert(profile.automatic_rotation == 1);
    assert(profile.force_feedback_strength == 35);
    assert(profile.vibration_strength == 10);
    assert(profile.brake_indicator_level == 101);
    assert(profile.force_scale == TUNING_FORCE_SCALE_PEAK);
    assert(profile.steering_deadzone == 0);
    assert(profile.drift_compensation == 0);
    assert(profile.force_effect_strength == 10);
    assert(profile.spring_effect_strength == 10);
    assert(profile.damper_effect_strength == 10);
    assert(profile.natural_damper == 50);
    assert(profile.natural_friction == 0);
    assert(profile.brake_force == 50);
    assert(profile.alternate_brake_force == 50);
    assert(profile.force_effect_intensity == 100);
    assert(profile.multi_position_mode == TUNING_MULTI_POSITION_AUTOMATIC);
    assert(profile.paddle_mode == TUNING_CLUTCH_BRAKE);
    assert(profile.interpolation_filter == 6);
    assert(profile.natural_inertia == 0);
    assert(profile.full_force_enabled == 0);
    assert(profile.button_illumination_enabled == 1);
    assert(profile.display_rotation_enabled == 1);
    assert(profile.brake_pedal_curve == TUNING_PEDAL_CURVE_LINEAR);
    assert(profile.clutch_pedal_curve == TUNING_PEDAL_CURVE_LINEAR);
    assert(profile.throttle_pedal_curve == TUNING_PEDAL_CURVE_LINEAR);
}

static void test_numeric_limits(void) {
    TuningProfile profile;
    tuning_profile_defaults(&profile);
    profile.rotation_degrees = UINT16_MAX;
    profile.force_feedback_strength = UINT8_MAX;
    profile.vibration_strength = UINT8_MAX;
    profile.brake_indicator_level = 0;
    profile.steering_deadzone = UINT8_MAX;
    profile.force_effect_strength = UINT8_MAX;
    profile.spring_effect_strength = UINT8_MAX;
    profile.damper_effect_strength = UINT8_MAX;
    profile.natural_damper = UINT8_MAX;
    profile.natural_friction = UINT8_MAX;
    profile.brake_force = UINT8_MAX;
    profile.alternate_brake_force = UINT8_MAX;
    profile.force_effect_intensity = UINT8_MAX;
    profile.interpolation_filter = UINT8_MAX;
    profile.natural_inertia = UINT8_MAX;

    tuning_profile_normalize(&profile);
    assert(profile.rotation_degrees == TUNING_ROTATION_MAX_DEGREES);
    assert(profile.force_feedback_strength == 100);

    profile.force_feedback_strength = 101;
    tuning_profile_normalize(&profile);
    assert(profile.force_feedback_strength == 101);
    assert(profile.vibration_strength == TUNING_VIBRATION_STRENGTH_MAX);
    assert(profile.brake_indicator_level == 1);
    assert(profile.steering_deadzone == 10);
    assert(profile.force_effect_strength == 12);
    assert(profile.spring_effect_strength == 12);
    assert(profile.damper_effect_strength == 12);
    assert(profile.natural_damper == 100);
    assert(profile.natural_friction == 100);
    assert(profile.brake_force == 100);
    assert(profile.alternate_brake_force == 100);
    assert(profile.force_effect_intensity == 100);
    assert(profile.interpolation_filter == 20);
    assert(profile.natural_inertia == 100);
}

static void test_rotation_normalization(void) {
    TuningProfile profile;
    tuning_profile_defaults(&profile);

    profile.rotation_degrees = 89;
    tuning_profile_normalize(&profile);
    assert(profile.rotation_degrees == TUNING_ROTATION_MIN_DEGREES);

    profile.rotation_degrees = 1087;
    tuning_profile_normalize(&profile);
    assert(profile.rotation_degrees == 1080);
}

static void test_modes(void) {
    TuningProfile profile;
    tuning_profile_defaults(&profile);
    profile.automatic_rotation = 2;
    profile.vibration_strength = 200;
    profile.force_scale = (TuningForceScale)10;
    profile.drift_compensation = 2;
    profile.multi_position_mode = (TuningMultiPositionMode)10;
    profile.paddle_mode = (TuningPaddleMode)10;
    profile.full_force_enabled = 2;
    profile.button_illumination_enabled = 2;
    profile.display_rotation_enabled = 2;
    profile.brake_pedal_curve = (TuningPedalCurve)10;
    profile.clutch_pedal_curve = (TuningPedalCurve)10;
    profile.throttle_pedal_curve = (TuningPedalCurve)10;

    tuning_profile_normalize(&profile);
    assert(profile.automatic_rotation == 1);
    assert(profile.vibration_strength == TUNING_VIBRATION_STRENGTH_MAX);
    assert(profile.force_scale == TUNING_FORCE_SCALE_LINEAR);
    assert(profile.drift_compensation == 1);
    assert(profile.multi_position_mode == TUNING_MULTI_POSITION_AUTOMATIC);
    assert(profile.paddle_mode == TUNING_CLUTCH_BRAKE);
    assert(profile.full_force_enabled == 1);
    assert(profile.button_illumination_enabled == 1);
    assert(profile.display_rotation_enabled == 1);
    assert(profile.brake_pedal_curve == TUNING_PEDAL_CURVE_LINEAR);
    assert(profile.clutch_pedal_curve == TUNING_PEDAL_CURVE_LINEAR);
    assert(profile.throttle_pedal_curve == TUNING_PEDAL_CURVE_LINEAR);
}

int main(void) {
    test_defaults();
    test_numeric_limits();
    test_rotation_normalization();
    test_modes();
    return 0;
}
