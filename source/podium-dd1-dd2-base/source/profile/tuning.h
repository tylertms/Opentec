#ifndef OPENTEC_BASE_PROFILE_TUNING_H
#define OPENTEC_BASE_PROFILE_TUNING_H

#include <stdint.h>

/** @brief Supported limits for common tuning values. */
enum {
    TUNING_ROTATION_MIN_DEGREES = 90,   /**< Minimum lock-to-lock steering range. */
    TUNING_ROTATION_MAX_DEGREES = 2520, /**< Maximum lock-to-lock steering range. */
    TUNING_ROTATION_STEP_DEGREES = 10,  /**< Concrete steering-range increment. */
    TUNING_VIBRATION_STRENGTH_MAX = 10, /**< Maximum vibration strength. */
};

/** @brief Force-feedback scaling strategy. */
typedef enum {
    TUNING_FORCE_SCALE_LINEAR, /**< Scale force effects linearly. */
    TUNING_FORCE_SCALE_PEAK,   /**< Preserve peak force while scaling effects. */
} TuningForceScale;

/** @brief Multi-position switch input mode. */
typedef enum {
    TUNING_MULTI_POSITION_ENCODER,   /**< Use encoder input. */
    TUNING_MULTI_POSITION_PULSE,     /**< Use pulse input. */
    TUNING_MULTI_POSITION_CONSTANT,  /**< Use constant input. */
    TUNING_MULTI_POSITION_AUTOMATIC, /**< Select the mode automatically. */
} TuningMultiPositionMode;

/** @brief Analogue paddle assignment mode. */
typedef enum {
    TUNING_CLUTCH_BRAKE = 1, /**< Use the clutch bite-point paddle assignment. */
    TUNING_CLUTCH_HANDBRAKE, /**< Assign paddles to clutch and handbrake. */
    TUNING_BRAKE_THROTTLE,   /**< Assign paddles to brake and throttle. */
    TUNING_DUAL_ANALOG,      /**< Use dual analogue paddle inputs. */
} TuningPaddleMode;

/** @brief Pedal response curve. */
typedef enum {
    TUNING_PEDAL_CURVE_ONE,         /**< Use custom pedal curve one. */
    TUNING_PEDAL_CURVE_TWO,         /**< Use custom pedal curve two. */
    TUNING_PEDAL_CURVE_THREE,       /**< Use custom pedal curve three. */
    TUNING_PEDAL_CURVE_LINEAR,      /**< Linear response curve. */
    TUNING_PEDAL_CURVE_PROGRESSIVE, /**< Progressive response curve. */
    TUNING_PEDAL_CURVE_DEGREES,     /**< Use the degressive response curve. */
} TuningPedalCurve;

/** @brief All retained tuning values for one setup. */
typedef struct {
    uint16_t rotation_degrees;       /**< Concrete lock-to-lock steering range. */
    uint8_t automatic_rotation;      /**< Nonzero when steering range is automatic. */
    uint8_t force_feedback_strength; /**< Force-feedback strength, or 101 for automatic. */
    uint8_t vibration_strength;      /**< Vibration strength from zero to ten. */
    uint8_t brake_indicator_level;   /**< Brake indicator level, or 101 when disabled. */
    TuningForceScale force_scale;    /**< Force-feedback scaling strategy. */
    uint8_t steering_deadzone;       /**< Steering deadzone level from zero to ten. */
    uint8_t drift_compensation;      /**< Nonzero when drift compensation is enabled. */
    uint8_t force_effect_strength;   /**< Force-effect strength from zero to twelve. */
    uint8_t spring_effect_strength;  /**< Spring-effect strength from zero to twelve. */
    uint8_t damper_effect_strength;  /**< Damper-effect strength from zero to twelve. */
    uint8_t natural_damper;          /**< Natural damper percentage. */
    uint8_t natural_friction;        /**< Natural friction percentage. */
    uint8_t brake_force;             /**< Primary brake force percentage. */
    uint8_t alternate_brake_force;   /**< Alternate brake force percentage. */
    uint8_t force_effect_intensity;  /**< Force-effect intensity percentage. */
    TuningMultiPositionMode multi_position_mode; /**< Multi-position switch mode. */
    TuningPaddleMode paddle_mode;                /**< Analogue paddle assignment mode. */
    uint8_t interpolation_filter;                /**< Interpolation filter level. */
    uint8_t natural_inertia;                     /**< Natural inertia percentage. */
    uint8_t full_force_enabled;                  /**< Nonzero when full force is enabled. */
    uint8_t button_illumination_enabled;   /**< Nonzero when button illumination is enabled. */
    uint8_t display_rotation_enabled;      /**< Nonzero when display rotation is enabled. */
    TuningPedalCurve brake_pedal_curve;    /**< Brake pedal response curve. */
    TuningPedalCurve clutch_pedal_curve;   /**< Clutch pedal response curve. */
    TuningPedalCurve throttle_pedal_curve; /**< Throttle pedal response curve. */
} TuningProfile;

/**
 * @brief Restores one tuning profile to device defaults.
 *
 * Initializes every profile field, including automatic steering selection and its fallback range.
 *
 * @param[out] profile Profile to initialize.
 */
void tuning_profile_defaults(TuningProfile *profile);

/**
 * @brief Normalizes one tuning profile.
 *
 * Clamps numeric fields and maps unsupported enum and flag values to supported representations.
 *
 * @param[in,out] profile Profile to normalize.
 */
void tuning_profile_normalize(TuningProfile *profile);

#endif
