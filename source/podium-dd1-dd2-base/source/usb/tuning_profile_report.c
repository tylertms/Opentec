#include "usb/tuning_profile_report.h"

#include <stddef.h>
#include <stdint.h>

#include "usb/device.h"

enum {
    AUTOMATIC_ROTATION_CODE = 126,
    ROTATION_ENCODING_BIAS = 127,
    ROTATION_UNIT_DEGREES = 10,
    RESPONSE_COMMAND = 3,
    RESPONSE_STANDARD_MODE_MASK = 0x80,
};

static uint8_t encode_rotation(const TuningProfile *profile) {
    if (profile->automatic_rotation != 0) {
        return AUTOMATIC_ROTATION_CODE;
    }
    return (uint8_t)(int8_t)((int16_t)(profile->rotation_degrees / ROTATION_UNIT_DEGREES) -
                             ROTATION_ENCODING_BIAS);
}

static void decode_rotation(uint8_t value, TuningProfile *profile) {
    profile->automatic_rotation = value == AUTOMATIC_ROTATION_CODE;
    if (!profile->automatic_rotation) {
        profile->rotation_degrees =
            (uint16_t)((int16_t)(int8_t)value + ROTATION_ENCODING_BIAS) * ROTATION_UNIT_DEGREES;
    }
}

/**
 * @brief Decodes the device-control tuning-profile values.
 *
 * Applies the signed, biased steering-range representation and maps the remaining values to the
 * logical tuning profile. Automatic range selection keeps the profile's current concrete range
 * because that range is supplied independently at runtime. The decoded values are normalized.
 *
 * @param[in] input Twenty-five profile values following the profile selector.
 * @param[in,out] profile Logical tuning profile to update.
 * @return True when both pointers are valid.
 */
bool usb_tuning_profile_report_decode(const uint8_t input[USB_TUNING_PROFILE_VALUE_COUNT],
                                      TuningProfile *profile) {
    if (input == NULL || profile == NULL) {
        return false;
    }

    decode_rotation(input[0], profile);
    profile->force_feedback_strength = input[1];
    profile->vibration_strength = input[2];
    profile->brake_indicator_level = input[3];
    profile->force_scale = (TuningForceScale)input[4];
    profile->steering_deadzone = input[5];
    profile->drift_compensation = input[6];
    profile->force_effect_strength = input[7];
    profile->spring_effect_strength = input[8];
    profile->damper_effect_strength = input[9];
    profile->natural_damper = input[10];
    profile->natural_friction = input[11];
    profile->brake_force = input[12];
    profile->alternate_brake_force = input[13];
    profile->force_effect_intensity = input[14];
    profile->multi_position_mode = (TuningMultiPositionMode)input[15];
    profile->paddle_mode = (TuningPaddleMode)input[16];
    profile->interpolation_filter = input[17];
    profile->natural_inertia = input[18];
    profile->full_force_enabled = input[19];
    profile->button_illumination_enabled = input[20];
    profile->display_rotation_enabled = input[21];
    profile->brake_pedal_curve = (TuningPedalCurve)input[22];
    profile->clutch_pedal_curve = (TuningPedalCurve)input[23];
    profile->throttle_pedal_curve = (TuningPedalCurve)input[24];
    tuning_profile_normalize(profile);
    return true;
}

/**
 * @brief Encodes the device-control tuning-profile values.
 *
 * Converts the concrete steering range to the signed, biased ten-degree representation or writes
 * the automatic-range sentinel, then emits the remaining logical settings in protocol order.
 *
 * @param[in] profile Logical tuning profile.
 * @param[out] output Twenty-five encoded profile values.
 */
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

/**
 * @brief Encodes a complete tuning-profile response.
 *
 * Clears the native vendor report, writes the FF 03 header, combines the one-based active profile
 * with the Standard-mode flag, and appends the active profile's twenty-five values.
 *
 * @param[in] bank Tuning profiles and active Standard or Advanced mode.
 * @param[out] output Encoded 64-byte vendor report.
 */
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
