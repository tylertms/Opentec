#include "profile/record.h"

#include <stdbool.h>
#include <stdint.h>

/** @brief Internal constants for the tuning-profile record format. */
enum {
    RECORD_MAGIC_0 = 'O',             /**< First magic byte. */
    RECORD_MAGIC_1 = 'T',             /**< Second magic byte. */
    RECORD_MAGIC_2 = 'P',             /**< Third magic byte. */
    RECORD_MAGIC_3 = 'F',             /**< Fourth magic byte. */
    RECORD_LEGACY_VERSION = 1,        /**< Legacy supported record version. */
    RECORD_STANDARD_MODE_MASK = 0x80, /**< Header bit indicating Standard mode. */
    RECORD_SLOT_COUNT_MASK = 0x7f,    /**< Header mask for the retained slot count. */
    RECORD_DATA_SIZE =
        TUNING_PROFILE_RECORD_SIZE -
        TUNING_PROFILE_RECORD_CHECKSUM_SIZE, /**< Bytes covered by the record checksum. */
};

/**
 * @brief Calculates a tuning-profile record checksum.
 *
 * Applies non-reflected CRC-16/CCITT with seed 0xFFFF to the requested record bytes.
 *
 * @param[in] data Record bytes to checksum.
 * @param[in] size Number of input bytes.
 * @return Calculated CRC-16 value.
 */
static uint16_t record_checksum(const uint8_t *data, uint16_t size) {
    uint16_t checksum = UINT16_C(0xffff);
    for (uint16_t index = 0; index < size; index++) {
        checksum ^= (uint16_t)data[index] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            checksum = (checksum & UINT16_C(0x8000)) != 0
                           ? (uint16_t)((checksum << 1) ^ UINT16_C(0x1021))
                           : (uint16_t)(checksum << 1);
        }
    }
    return checksum;
}

/**
 * @brief Writes one byte to a tuning-profile record.
 *
 * Stores the value at the current cursor and advances past it.
 *
 * @param[in,out] output Record receiving the value.
 * @param[in] cursor Current write offset.
 * @param[in] value Byte to write.
 * @return Offset following the written byte.
 */
static uint16_t write_u8(uint8_t *output, uint16_t cursor, uint8_t value) {
    output[cursor] = value;
    return cursor + 1;
}

/**
 * @brief Writes one little-endian 16-bit record value.
 *
 * Stores the low byte followed by the high byte and advances past both.
 *
 * @param[in,out] output Record receiving the value.
 * @param[in] cursor Current write offset.
 * @param[in] value Value to write.
 * @return Offset following the written value.
 */
static uint16_t write_u16(uint8_t *output, uint16_t cursor, uint16_t value) {
    cursor = write_u8(output, cursor, (uint8_t)value);
    return write_u8(output, cursor, (uint8_t)(value >> 8));
}

/**
 * @brief Reads one little-endian 16-bit record value.
 *
 * Combines the two bytes at the supplied offset without alignment requirements.
 *
 * @param[in] input Encoded tuning-profile record.
 * @param[in] cursor Current read offset.
 * @return Decoded unsigned value.
 */
static uint16_t read_u16(const uint8_t *input, uint16_t cursor) {
    return (uint16_t)input[cursor] | ((uint16_t)input[cursor + 1] << 8);
}

/**
 * @brief Writes one logical tuning profile to a record.
 *
 * Serializes steering range and every retained one-byte tuning field in the stable record order.
 *
 * @param[in,out] output Record receiving the profile.
 * @param[in] cursor Current write offset.
 * @param[in] profile Logical tuning profile to encode.
 * @return Offset following the encoded profile.
 */
static uint16_t write_profile(uint8_t *output, uint16_t cursor, const TuningProfile *profile) {
    cursor = write_u16(output, cursor, profile->rotation_degrees);
    cursor = write_u8(output, cursor, profile->automatic_rotation);
    cursor = write_u8(output, cursor, profile->force_feedback_strength);
    cursor = write_u8(output, cursor, profile->vibration_strength);
    cursor = write_u8(output, cursor, profile->brake_indicator_level);
    cursor = write_u8(output, cursor, (uint8_t)profile->force_scale);
    cursor = write_u8(output, cursor, profile->steering_deadzone);
    cursor = write_u8(output, cursor, profile->drift_compensation);
    cursor = write_u8(output, cursor, profile->force_effect_strength);
    cursor = write_u8(output, cursor, profile->spring_effect_strength);
    cursor = write_u8(output, cursor, profile->damper_effect_strength);
    cursor = write_u8(output, cursor, profile->natural_damper);
    cursor = write_u8(output, cursor, profile->natural_friction);
    cursor = write_u8(output, cursor, profile->brake_force);
    cursor = write_u8(output, cursor, profile->alternate_brake_force);
    cursor = write_u8(output, cursor, profile->force_effect_intensity);
    cursor = write_u8(output, cursor, (uint8_t)profile->multi_position_mode);
    cursor = write_u8(output, cursor, (uint8_t)profile->paddle_mode);
    cursor = write_u8(output, cursor, profile->interpolation_filter);
    cursor = write_u8(output, cursor, profile->natural_inertia);
    cursor = write_u8(output, cursor, profile->full_force_enabled);
    cursor = write_u8(output, cursor, profile->button_illumination_enabled);
    cursor = write_u8(output, cursor, profile->display_rotation_enabled);
    cursor = write_u8(output, cursor, (uint8_t)profile->brake_pedal_curve);
    cursor = write_u8(output, cursor, (uint8_t)profile->clutch_pedal_curve);
    return write_u8(output, cursor, (uint8_t)profile->throttle_pedal_curve);
}

/**
 * @brief Reads one logical tuning profile from a record.
 *
 * Decodes every retained field in stable record order and normalizes the resulting profile.
 *
 * @param[in] input Encoded tuning-profile record.
 * @param[in] cursor Current read offset.
 * @param[out] profile Decoded and normalized tuning profile.
 * @return Offset following the decoded profile.
 */
static uint16_t read_profile(const uint8_t *input, uint16_t cursor, TuningProfile *profile) {
    profile->rotation_degrees = read_u16(input, cursor);
    cursor += 2;
    profile->automatic_rotation = input[cursor++];
    profile->force_feedback_strength = input[cursor++];
    profile->vibration_strength = input[cursor++];
    profile->brake_indicator_level = input[cursor++];
    profile->force_scale = (TuningForceScale)input[cursor++];
    profile->steering_deadzone = input[cursor++];
    profile->drift_compensation = input[cursor++];
    profile->force_effect_strength = input[cursor++];
    profile->spring_effect_strength = input[cursor++];
    profile->damper_effect_strength = input[cursor++];
    profile->natural_damper = input[cursor++];
    profile->natural_friction = input[cursor++];
    profile->brake_force = input[cursor++];
    profile->alternate_brake_force = input[cursor++];
    profile->force_effect_intensity = input[cursor++];
    profile->multi_position_mode = (TuningMultiPositionMode)input[cursor++];
    profile->paddle_mode = (TuningPaddleMode)input[cursor++];
    profile->interpolation_filter = input[cursor++];
    profile->natural_inertia = input[cursor++];
    profile->full_force_enabled = input[cursor++];
    profile->button_illumination_enabled = input[cursor++];
    profile->display_rotation_enabled = input[cursor++];
    profile->brake_pedal_curve = (TuningPedalCurve)input[cursor++];
    profile->clutch_pedal_curve = (TuningPedalCurve)input[cursor++];
    profile->throttle_pedal_curve = (TuningPedalCurve)input[cursor++];
    tuning_profile_normalize(profile);
    return cursor;
}

bool tuning_profile_record_encode(const TuningProfileBank *bank,
                                  uint8_t output[TUNING_PROFILE_RECORD_SIZE]) {
    if (bank->selected_slot >= TUNING_PROFILE_SLOT_COUNT ||
        bank->active_slot >= TUNING_PROFILE_SLOT_COUNT) {
        return false;
    }

    uint16_t cursor = 0;
    cursor = write_u8(output, cursor, RECORD_MAGIC_0);
    cursor = write_u8(output, cursor, RECORD_MAGIC_1);
    cursor = write_u8(output, cursor, RECORD_MAGIC_2);
    cursor = write_u8(output, cursor, RECORD_MAGIC_3);
    cursor = write_u8(output, cursor, TUNING_PROFILE_RECORD_VERSION);
    cursor = write_u8(output, cursor, bank->selected_slot);
    cursor = write_u8(output, cursor, bank->active_slot);
    uint8_t flags = (uint8_t)TUNING_PROFILE_SLOT_COUNT;
    if (bank->standard_mode_enabled) {
        flags |= (uint8_t)RECORD_STANDARD_MODE_MASK;
    }
    cursor = write_u8(output, cursor, flags);

    for (uint8_t slot = 0; slot < TUNING_PROFILE_SLOT_COUNT; slot++) {
        cursor = write_profile(output, cursor, &bank->slots[slot]);
    }

    cursor = write_u16(output, cursor, record_checksum(output, RECORD_DATA_SIZE));
    return cursor == TUNING_PROFILE_RECORD_SIZE;
}

bool tuning_profile_record_decode(const uint8_t input[TUNING_PROFILE_RECORD_SIZE],
                                  TuningProfileBank *bank) {
    uint16_t stored_checksum =
        (uint16_t)input[RECORD_DATA_SIZE] | ((uint16_t)input[RECORD_DATA_SIZE + 1] << 8);
    if (input[0] != RECORD_MAGIC_0 || input[1] != RECORD_MAGIC_1 || input[2] != RECORD_MAGIC_2 ||
        input[3] != RECORD_MAGIC_3 ||
        (input[4] != RECORD_LEGACY_VERSION && input[4] != TUNING_PROFILE_RECORD_VERSION) ||
        (input[7] & RECORD_SLOT_COUNT_MASK) != TUNING_PROFILE_SLOT_COUNT ||
        input[5] >= TUNING_PROFILE_SLOT_COUNT || input[6] >= TUNING_PROFILE_SLOT_COUNT ||
        stored_checksum != record_checksum(input, RECORD_DATA_SIZE)) {
        return false;
    }

    TuningProfileBank decoded;
    decoded.selected_slot = input[5];
    decoded.active_slot = input[6];
    decoded.standard_mode_enabled =
        input[4] == RECORD_LEGACY_VERSION || (input[7] & RECORD_STANDARD_MODE_MASK) != 0;
    uint16_t cursor = TUNING_PROFILE_RECORD_HEADER_SIZE;
    for (uint8_t slot = 0; slot < TUNING_PROFILE_SLOT_COUNT; slot++) {
        cursor = read_profile(input, cursor, &decoded.slots[slot]);
    }
    *bank = decoded;
    return cursor == RECORD_DATA_SIZE;
}
