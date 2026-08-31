#ifndef OPENTEC_BASE_PROFILE_TUNING_H
#define OPENTEC_BASE_PROFILE_TUNING_H

#include <stdint.h>

enum {
    TUNING_ROTATION_MIN_DEGREES = 90,
    TUNING_ROTATION_MAX_DEGREES = 2520,
    TUNING_ROTATION_STEP_DEGREES = 10,
    TUNING_VIBRATION_STRENGTH_MAX = 10,
};

typedef enum {
    TUNING_FORCE_SCALE_LINEAR,
    TUNING_FORCE_SCALE_PEAK,
} TuningForceScale;

typedef enum {
    TUNING_MULTI_POSITION_ENCODER,
    TUNING_MULTI_POSITION_PULSE,
    TUNING_MULTI_POSITION_CONSTANT,
    TUNING_MULTI_POSITION_AUTOMATIC,
} TuningMultiPositionMode;

typedef enum {
    TUNING_CLUTCH_BRAKE = 1,
    TUNING_CLUTCH_HANDBRAKE,
    TUNING_BRAKE_THROTTLE,
    TUNING_DUAL_ANALOG,
} TuningPaddleMode;

typedef enum {
    TUNING_PEDAL_CURVE_ONE,
    TUNING_PEDAL_CURVE_TWO,
    TUNING_PEDAL_CURVE_THREE,
    TUNING_PEDAL_CURVE_LINEAR,
    TUNING_PEDAL_CURVE_PROGRESSIVE,
    TUNING_PEDAL_CURVE_DEGREES,
} TuningPedalCurve;

typedef struct {
    uint16_t rotation_degrees;
    uint8_t automatic_rotation;
    uint8_t force_feedback_strength;
    uint8_t vibration_strength;
    uint8_t brake_indicator_level;
    TuningForceScale force_scale;
    uint8_t steering_deadzone;
    uint8_t drift_compensation;
    uint8_t force_effect_strength;
    uint8_t spring_effect_strength;
    uint8_t damper_effect_strength;
    uint8_t natural_damper;
    uint8_t natural_friction;
    uint8_t brake_force;
    uint8_t alternate_brake_force;
    uint8_t force_effect_intensity;
    TuningMultiPositionMode multi_position_mode;
    TuningPaddleMode paddle_mode;
    uint8_t interpolation_filter;
    uint8_t natural_inertia;
    uint8_t full_force_enabled;
    uint8_t button_illumination_enabled;
    uint8_t display_rotation_enabled;
    TuningPedalCurve brake_pedal_curve;
    TuningPedalCurve clutch_pedal_curve;
    TuningPedalCurve throttle_pedal_curve;
} TuningProfile;

void tuning_profile_defaults(TuningProfile *profile);
void tuning_profile_normalize(TuningProfile *profile);

#endif
