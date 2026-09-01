#include "usb/tuning_profile_report.h"

#include <stddef.h>
#include <stdint.h>

#include "usb/device.h"

/** @brief Tuning-profile report encoding constants. */
enum {
    AUTOMATIC_ROTATION_CODE = 126, /**< Wire value selecting automatic steering range. */
    ROTATION_ENCODING_BIAS = 127,  /**< Bias applied to signed ten-degree steering-range values. */
    ROTATION_UNIT_DEGREES = 10,    /**< Steering-range encoding unit in degrees. */
    RESPONSE_COMMAND = 3,          /**< Vendor command identifier for profile responses. */
    RESPONSE_STANDARD_MODE_MASK = 0x80, /**< Response bit indicating Standard mode. */
};

/**
 * @brief Encodes the steering range for a tuning-profile report.
 *
 * Emits the automatic-range sentinel or converts the concrete ten-degree range to its signed,
 * biased one-byte representation.
 *
 * @param[in] profile Logical tuning profile.
 * @return The encoded steering-range value.
 */
static uint8_t encode_rotation(const TuningProfile *profile) {
    if (profile->automatic_rotation != 0) {
        return AUTOMATIC_ROTATION_CODE;
    }
    return (uint8_t)(int8_t)((int16_t)(profile->rotation_degrees / ROTATION_UNIT_DEGREES) -
                             ROTATION_ENCODING_BIAS);
}

/**
 * @brief Decodes the steering range from a tuning-profile report.
 *
 * Selects automatic range without changing the current concrete range, or converts a valid manual
 * signed, biased value to degrees. Invalid manual values leave the profile unchanged.
 *
 * @param[in] value Encoded steering-range value.
 * @param[in,out] profile Logical tuning profile to update.
 */
static void decode_rotation(uint8_t value, TuningProfile *profile) {
    if (value == AUTOMATIC_ROTATION_CODE) {
        profile->automatic_rotation = 1;
        return;
    }
    uint16_t degrees =
        (uint16_t)((int16_t)(int8_t)value + ROTATION_ENCODING_BIAS) * ROTATION_UNIT_DEGREES;
    if (degrees >= TUNING_ROTATION_MIN_DEGREES && degrees <= TUNING_ROTATION_MAX_DEGREES) {
        profile->automatic_rotation = 0;
        profile->rotation_degrees = degrees;
    }
}

/**
 * @brief Retains a bounded tuning value.
 *
 * Accepts the candidate only when it lies within the inclusive range.
 *
 * @param[in] value Candidate encoded value.
 * @param[in] current Current value retained on rejection.
 * @param[in] minimum Inclusive minimum.
 * @param[in] maximum Inclusive maximum.
 * @return Accepted candidate or the current value when out of range.
 */
static uint8_t retain_range(uint8_t value, uint8_t current, uint8_t minimum, uint8_t maximum) {
    return value >= minimum && value <= maximum ? value : current;
}

/**
 * @brief Retains an encoded Boolean tuning value.
 *
 * Accepts zero or one and preserves the current value for every other encoding.
 *
 * @param[in] value Candidate encoded Boolean.
 * @param[in] current Current value retained on rejection.
 * @return Accepted candidate or the current value when invalid.
 */
static uint8_t retain_boolean(uint8_t value, uint8_t current) {
    return value <= 1 ? value : current;
}

bool usb_tuning_profile_report_decode(const uint8_t input[USB_TUNING_PROFILE_VALUE_COUNT],
                                      TuningProfile *profile) {
    if (input == NULL || profile == NULL) {
        return false;
    }

    decode_rotation(input[0], profile);
    profile->force_feedback_strength =
        input[1] <= 101 ? input[1] : profile->force_feedback_strength;
    profile->vibration_strength = retain_range(input[2], profile->vibration_strength, 0, 10);
    profile->brake_indicator_level = retain_range(input[3], profile->brake_indicator_level, 1, 101);
    profile->force_scale =
        (TuningForceScale)retain_range(input[4], (uint8_t)profile->force_scale,
                                       TUNING_FORCE_SCALE_LINEAR, TUNING_FORCE_SCALE_PEAK);
    profile->steering_deadzone = retain_range(input[5], profile->steering_deadzone, 0, 10);
    profile->drift_compensation = retain_boolean(input[6], profile->drift_compensation);
    profile->force_effect_strength = retain_range(input[7], profile->force_effect_strength, 0, 12);
    profile->spring_effect_strength =
        retain_range(input[8], profile->spring_effect_strength, 0, 12);
    profile->damper_effect_strength =
        retain_range(input[9], profile->damper_effect_strength, 0, 12);
    profile->natural_damper = retain_range(input[10], profile->natural_damper, 0, 100);
    profile->natural_friction = retain_range(input[11], profile->natural_friction, 0, 100);
    profile->brake_force = retain_range(input[12], profile->brake_force, 0, 100);
    profile->alternate_brake_force =
        retain_range(input[13], profile->alternate_brake_force, 0, 100);
    profile->force_effect_intensity =
        retain_range(input[14], profile->force_effect_intensity, 0, 100);
    profile->multi_position_mode = (TuningMultiPositionMode)retain_range(
        input[15], (uint8_t)profile->multi_position_mode, TUNING_MULTI_POSITION_ENCODER,
        TUNING_MULTI_POSITION_AUTOMATIC);
    profile->paddle_mode = (TuningPaddleMode)retain_range(input[16], (uint8_t)profile->paddle_mode,
                                                          TUNING_CLUTCH_BRAKE, TUNING_DUAL_ANALOG);
    profile->interpolation_filter = retain_range(input[17], profile->interpolation_filter, 0, 20);
    profile->natural_inertia = retain_range(input[18], profile->natural_inertia, 0, 100);
    profile->full_force_enabled = retain_boolean(input[19], profile->full_force_enabled);
    profile->button_illumination_enabled =
        retain_boolean(input[20], profile->button_illumination_enabled);
    profile->display_rotation_enabled =
        retain_boolean(input[21], profile->display_rotation_enabled);
    profile->brake_pedal_curve =
        (TuningPedalCurve)retain_range(input[22], (uint8_t)profile->brake_pedal_curve,
                                       TUNING_PEDAL_CURVE_ONE, TUNING_PEDAL_CURVE_DEGREES);
    profile->clutch_pedal_curve =
        (TuningPedalCurve)retain_range(input[23], (uint8_t)profile->clutch_pedal_curve,
                                       TUNING_PEDAL_CURVE_ONE, TUNING_PEDAL_CURVE_DEGREES);
    profile->throttle_pedal_curve =
        (TuningPedalCurve)retain_range(input[24], (uint8_t)profile->throttle_pedal_curve,
                                       TUNING_PEDAL_CURVE_ONE, TUNING_PEDAL_CURVE_DEGREES);
    return true;
}

void usb_tuning_profile_report_encode(const TuningProfile *profile,
                                      uint8_t output[USB_TUNING_PROFILE_VALUE_COUNT]) {
    output[0] = encode_rotation(profile);
    output[1] = profile->force_feedback_strength;
    output[2] = profile->vibration_strength;
    output[3] = profile->brake_indicator_level;
    output[4] = (uint8_t)profile->force_scale;
    output[5] = profile->steering_deadzone;
    output[6] = profile->drift_compensation;
    output[7] = profile->force_effect_strength;
    output[8] = profile->spring_effect_strength;
    output[9] = profile->damper_effect_strength;
    output[10] = profile->natural_damper;
    output[11] = profile->natural_friction;
    output[12] = profile->brake_force;
    output[13] = profile->alternate_brake_force;
    output[14] = profile->force_effect_intensity;
    output[15] = (uint8_t)profile->multi_position_mode;
    output[16] = (uint8_t)profile->paddle_mode;
    output[17] = profile->interpolation_filter;
    output[18] = profile->natural_inertia;
    output[19] = profile->full_force_enabled;
    output[20] = profile->button_illumination_enabled;
    output[21] = profile->display_rotation_enabled;
    output[22] = (uint8_t)profile->brake_pedal_curve;
    output[23] = (uint8_t)profile->clutch_pedal_curve;
    output[24] = (uint8_t)profile->throttle_pedal_curve;
}

void usb_tuning_profile_report_encode_response(const TuningProfileBank *bank,
                                               uint8_t output[USB_DEVICE_REPORT_SIZE]) {
    for (uint8_t index = 0; index < USB_DEVICE_REPORT_SIZE; index++) {
        output[index] = 0;
    }
    output[0] = UINT8_MAX;
    output[1] = RESPONSE_COMMAND;
    output[2] = (uint8_t)(bank->active_slot + 1);
    if (bank->standard_mode_enabled) {
        output[2] |= RESPONSE_STANDARD_MODE_MASK;
    }
    usb_tuning_profile_report_encode(tuning_profile_bank_active(bank), &output[3]);
}
