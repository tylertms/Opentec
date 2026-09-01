#include "motor/tuning_parameter.h"

#include <stdint.h>

/**
 * @brief Clamps an unsigned parameter to its supported maximum.
 *
 * Preserves values within the supported range and replaces larger values with the maximum.
 *
 * @param[in] value Parameter value to constrain.
 * @param[in] maximum Largest supported value.
 * @return Constrained parameter value.
 */
static uint8_t clamp(uint8_t value, uint8_t maximum) { return value > maximum ? maximum : value; }

/**
 * @brief Builds a one-byte motor parameter write.
 *
 * Stores the parameter address and value, selects a one-byte transfer, and clears the unused data
 * byte.
 *
 * @param[out] write Motor parameter write to populate.
 * @param[in] address Motor parameter address.
 * @param[in] value One-byte parameter value.
 */
static void encode_u8(MotorParameterWrite *write, uint8_t address, uint8_t value) {
    write->address = address;
    write->length = 1;
    write->data[0] = value;
    write->data[1] = 0;
}

/**
 * @brief Builds a little-endian two-byte motor parameter write.
 *
 * Stores the parameter address and splits the value into the two transmitted data bytes.
 *
 * @param[out] write Motor parameter write to populate.
 * @param[in] address Motor parameter address.
 * @param[in] value Two-byte parameter value.
 */
static void encode_u16(MotorParameterWrite *write, uint8_t address, uint16_t value) {
    write->address = address;
    write->length = 2;
    write->data[0] = (uint8_t)value;
    write->data[1] = (uint8_t)(value >> 8);
}

/**
 * @brief Scales a percentage to the full unsigned byte range.
 *
 * Constrains the input to 100 percent and applies integer scaling with truncation.
 *
 * @param[in] value Percentage to scale.
 * @return Scaled value from zero through 255.
 */
static uint8_t scale_percent_to_u8(uint8_t value) {
    return (uint8_t)((uint16_t)clamp(value, 100) * UINT8_MAX / 100);
}

/**
 * @brief Calculates the active natural-friction motor value.
 *
 * Scales the profile percentage to sixteen bits, then applies the force-feedback ramp and hardware
 * strength percentages in sequence.
 *
 * @param[in] profile Active tuning profile.
 * @param[in] context Runtime force-feedback scaling context.
 * @return Scaled sixteen-bit natural-friction value.
 */
static uint16_t scale_friction(const TuningProfile *profile, const MotorTuningContext *context) {
    uint32_t friction = (uint32_t)clamp(profile->natural_friction, 100) * UINT16_MAX / 100;
    friction = friction * clamp(context->ramp_percent, 100) / 100;
    friction = friction * clamp(context->strength_percent, 100) / 100;
    return (uint16_t)friction;
}

/**
 * @brief Encodes the motor interpolation-filter value.
 *
 * Reverses the zero-through-twenty tuning scale. Xbox mode mirrors values through ten for profile
 * settings zero through nine.
 *
 * @param[in] profile Active tuning profile.
 * @param[in] context Current operating-mode context.
 * @return Encoded interpolation-filter value.
 */
static uint8_t encode_interpolation_filter(const TuningProfile *profile,
                                           const MotorTuningContext *context) {
    uint8_t filter = clamp(profile->interpolation_filter, 20);
    if (context->xbox_mode != 0 && filter <= 9) {
        return 10 - filter;
    }
    return 20 - filter;
}

/**
 * @brief Encodes the active steering range for the motor controller.
 *
 * Selects the automatic or fixed range, constrains it to 90 through 2520 degrees, and converts it
 * to the signed, biased ten-degree representation used by motor parameter 0x20.
 *
 * @param[in] profile Active tuning profile.
 * @param[in] context Runtime automatic-range context.
 * @return Encoded steering sensitivity byte.
 */
static uint8_t encode_sensitivity(const TuningProfile *profile, const MotorTuningContext *context) {
    uint16_t degrees = profile->automatic_rotation != 0 ? context->automatic_rotation_degrees
                                                        : profile->rotation_degrees;
    if (degrees < 90) {
        degrees = 90;
    } else if (degrees > 2520) {
        degrees = 2520;
    }

    int16_t sensitivity =
        degrees < 1000 ? ((int16_t)degrees - 1270) / 10 : (int16_t)(degrees / 10) - 127;
    return (uint8_t)sensitivity;
}

/**
 * @brief Encodes one logical tuning setting as a motor parameter write.
 *
 * Maps supported settings to motor addresses 0x20 through 0x2a and applies each setting's width,
 * scaling, operating-mode rules, and controller capability requirements.
 *
 * @param[in] parameter Logical motor tuning setting to encode.
 * @param[in] profile Active tuning profile.
 * @param[in] context Runtime motor tuning context.
 * @param[out] write Encoded motor parameter write.
 * @return One when the parameter is supported; otherwise zero.
 */
uint8_t motor_tuning_parameter_encode(MotorTuningParameter parameter, const TuningProfile *profile,
                                      const MotorTuningContext *context,
                                      MotorParameterWrite *write) {
    if (context->extended_parameters == 0 && (parameter == MOTOR_TUNING_FORCE_FEEDBACK_SCALE ||
                                              parameter == MOTOR_TUNING_NATURAL_INERTIA ||
                                              parameter == MOTOR_TUNING_INTERPOLATION_FILTER)) {
        return 0;
    }

    switch (parameter) {
    case MOTOR_TUNING_SENSITIVITY:
        encode_u8(write, 0x20, encode_sensitivity(profile, context));
        return 1;
    case MOTOR_TUNING_FORCE_FEEDBACK_STRENGTH:
        encode_u8(write, 0x21, profile->force_feedback_strength);
        return 1;
    case MOTOR_TUNING_FORCE_FEEDBACK_SCALE:
        encode_u8(write, 0x22,
                  profile->force_scale == TUNING_FORCE_SCALE_LINEAR ||
                          context->calibration_active != 0
                      ? 0xaa
                      : 0);
        return 1;
    case MOTOR_TUNING_NATURAL_DAMPER:
        encode_u8(write, 0x23, scale_percent_to_u8(profile->natural_damper));
        return 1;
    case MOTOR_TUNING_NATURAL_FRICTION:
        encode_u16(write, 0x24, scale_friction(profile, context));
        return 1;
    case MOTOR_TUNING_NATURAL_INERTIA:
        encode_u8(write, 0x25, scale_percent_to_u8(profile->natural_inertia));
        return 1;
    case MOTOR_TUNING_INTERPOLATION_FILTER:
        encode_u8(write, 0x26, encode_interpolation_filter(profile, context));
        return 1;
    case MOTOR_TUNING_FORCE_EFFECT_INTENSITY:
        encode_u8(write, 0x27, clamp(profile->force_effect_intensity, 100));
        return 1;
    case MOTOR_TUNING_FORCE_EFFECT_STRENGTH:
        encode_u8(write, 0x28, clamp(profile->force_effect_strength, 12));
        return 1;
    case MOTOR_TUNING_SPRING_EFFECT_STRENGTH:
        encode_u8(write, 0x29, clamp(profile->spring_effect_strength, 12));
        return 1;
    case MOTOR_TUNING_DAMPER_EFFECT_STRENGTH:
        encode_u8(write, 0x2a, clamp(profile->damper_effect_strength, 12));
        return 1;
    case MOTOR_TUNING_PARAMETER_COUNT:
        return 0;
    }
    return 0;
}
