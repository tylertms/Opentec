#include "settings/persistence.h"

#include <stdbool.h>
#include <stdint.h>

#include "platform/storage.h"
#include "profile/record.h"
#include "settings/state.h"
#include "wheel/position.h"

enum {
    PERSISTENCE_MAGIC_0 = 'O',
    PERSISTENCE_MAGIC_1 = 'T',
    PERSISTENCE_MAGIC_2 = 'P',
    PERSISTENCE_MAGIC_3 = 'S',
    PERSISTENCE_PROFILE_ONLY_VERSION = 1,
    PERSISTENCE_WHEEL_REFERENCE_VERSION = 2,
    PERSISTENCE_H_PATTERN_VERSION = 3,
    PERSISTENCE_AUXILIARY_AXIS_VERSION = 4,
    PERSISTENCE_STEERING_LIMIT_VERSION = 5,
    PERSISTENCE_AUXILIARY_OUTPUT_VERSION = 6,
    PERSISTENCE_VERSION = 7,
    PERSISTENCE_HEADER_SIZE = 12,
    PERSISTENCE_REFERENCE_SIZE = 5,
    PERSISTENCE_H_PATTERN_SIZE = 17,
    PERSISTENCE_AUXILIARY_AXIS_SIZE = 5,
    PERSISTENCE_STEERING_LIMIT_SIZE = TUNING_PROFILE_SLOT_COUNT,
    PERSISTENCE_AUXILIARY_OUTPUT_SIZE = 1,
    PERSISTENCE_SECURITY_CODE_SIZE = 4,
    PERSISTENCE_REFERENCE_PAYLOAD_SIZE = TUNING_PROFILE_RECORD_SIZE + PERSISTENCE_REFERENCE_SIZE,
    PERSISTENCE_H_PATTERN_PAYLOAD_SIZE =
        PERSISTENCE_REFERENCE_PAYLOAD_SIZE + PERSISTENCE_H_PATTERN_SIZE,
    PERSISTENCE_AUXILIARY_AXIS_PAYLOAD_SIZE =
        PERSISTENCE_H_PATTERN_PAYLOAD_SIZE + PERSISTENCE_AUXILIARY_AXIS_SIZE,
    PERSISTENCE_STEERING_LIMIT_PAYLOAD_SIZE =
        PERSISTENCE_AUXILIARY_AXIS_PAYLOAD_SIZE + PERSISTENCE_STEERING_LIMIT_SIZE,
    PERSISTENCE_AUXILIARY_OUTPUT_PAYLOAD_SIZE =
        PERSISTENCE_STEERING_LIMIT_PAYLOAD_SIZE + PERSISTENCE_AUXILIARY_OUTPUT_SIZE,
    PERSISTENCE_PAYLOAD_SIZE =
        PERSISTENCE_AUXILIARY_OUTPUT_PAYLOAD_SIZE + PERSISTENCE_SECURITY_CODE_SIZE,
    PERSISTENCE_DATA_SIZE = PERSISTENCE_HEADER_SIZE + PERSISTENCE_PAYLOAD_SIZE,
    PERSISTENCE_CHECKSUM_SIZE = 2,
    PERSISTENCE_RECORD_SIZE = PERSISTENCE_DATA_SIZE + PERSISTENCE_CHECKSUM_SIZE,
};

typedef struct {
    BaseSettings settings;
    uint32_t generation;
    bool valid;
    bool needs_upgrade;
} StoredSettings;

/**
 * @brief Calculates the integrity code for retained settings.
 *
 * Applies the CRC-16/CCITT-FALSE polynomial to the requested byte sequence.
 *
 * @param[in] data Bytes to include in the calculation.
 * @param[in] size Number of bytes to process.
 * @return Sixteen-bit integrity code.
 */
static uint16_t persistence_checksum(const uint8_t *data, uint16_t size) {
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
 * @brief Reads a little-endian sixteen-bit value.
 *
 * Combines two consecutive bytes from the requested offset.
 *
 * @param[in] data Encoded byte sequence.
 * @param[in] offset Offset of the first byte.
 * @return Decoded sixteen-bit value.
 */
static uint16_t read_u16(const uint8_t *data, uint16_t offset) {
    return (uint16_t)data[offset] | ((uint16_t)data[offset + 1] << 8);
}

/**
 * @brief Reads a little-endian thirty-two-bit value.
 *
 * Combines four consecutive bytes from the requested offset.
 *
 * @param[in] data Encoded byte sequence.
 * @param[in] offset Offset of the first byte.
 * @return Decoded thirty-two-bit value.
 */
static uint32_t read_u32(const uint8_t *data, uint16_t offset) {
    return (uint32_t)data[offset] | ((uint32_t)data[offset + 1] << 8) |
           ((uint32_t)data[offset + 2] << 16) | ((uint32_t)data[offset + 3] << 24);
}

/**
 * @brief Writes a little-endian sixteen-bit value.
 *
 * Stores the low byte first at the requested offset.
 *
 * @param[out] data Encoded byte sequence.
 * @param[in] offset Offset of the first byte.
 * @param[in] value Value to encode.
 */
static void write_u16(uint8_t *data, uint16_t offset, uint16_t value) {
    data[offset] = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
}

/**
 * @brief Writes a little-endian thirty-two-bit value.
 *
 * Stores the four bytes from least significant to most significant.
 *
 * @param[out] data Encoded byte sequence.
 * @param[in] offset Offset of the first byte.
 * @param[in] value Value to encode.
 */
static void write_u32(uint8_t *data, uint16_t offset, uint32_t value) {
    data[offset] = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
    data[offset + 2] = (uint8_t)(value >> 16);
    data[offset + 3] = (uint8_t)(value >> 24);
}

/**
 * @brief Orders retained-record generations across counter rollover.
 *
 * Treats a nonzero forward distance shorter than half the counter range as newer.
 *
 * @param[in] candidate Generation to compare.
 * @param[in] reference Current generation.
 * @return True when the candidate follows the reference.
 */
static bool generation_is_newer(uint32_t candidate, uint32_t reference) {
    uint32_t distance = candidate - reference;
    return distance != 0 && distance < UINT32_C(0x80000000);
}

/**
 * @brief Validates a retained-settings record header.
 *
 * Accepts each supported payload version only with its exact encoded size, reserved byte, and
 * settings magic.
 *
 * @param[in] data Encoded settings record.
 * @param[in] payload_size Payload size read from the record header.
 * @return True when the header identifies a supported record layout.
 */
static bool header_valid(const uint8_t *data, uint16_t payload_size) {
    uint8_t version = data[4];
    bool current = version == PERSISTENCE_VERSION && payload_size == PERSISTENCE_PAYLOAD_SIZE;
    bool auxiliary_output = version == PERSISTENCE_AUXILIARY_OUTPUT_VERSION &&
                            payload_size == PERSISTENCE_AUXILIARY_OUTPUT_PAYLOAD_SIZE;
    bool steering_limits = version == PERSISTENCE_STEERING_LIMIT_VERSION &&
                           payload_size == PERSISTENCE_STEERING_LIMIT_PAYLOAD_SIZE;
    bool auxiliary_axis = version == PERSISTENCE_AUXILIARY_AXIS_VERSION &&
                          payload_size == PERSISTENCE_AUXILIARY_AXIS_PAYLOAD_SIZE;
    bool h_pattern = version == PERSISTENCE_H_PATTERN_VERSION &&
                     payload_size == PERSISTENCE_H_PATTERN_PAYLOAD_SIZE;
    bool wheel_reference = version == PERSISTENCE_WHEEL_REFERENCE_VERSION &&
                           payload_size == PERSISTENCE_REFERENCE_PAYLOAD_SIZE;
    bool profile_only =
        version == PERSISTENCE_PROFILE_ONLY_VERSION && payload_size == TUNING_PROFILE_RECORD_SIZE;
    return data[0] == PERSISTENCE_MAGIC_0 && data[1] == PERSISTENCE_MAGIC_1 &&
           data[2] == PERSISTENCE_MAGIC_2 && data[3] == PERSISTENCE_MAGIC_3 && data[5] == 0 &&
           (current || auxiliary_output || steering_limits || auxiliary_axis || h_pattern ||
            wheel_reference || profile_only);
}

/**
 * @brief Decodes retained H-pattern shifter calibration.
 *
 * Restores the availability flag and eight axis thresholds from the compact payload.
 *
 * @param[in] data Seventeen-byte calibration payload.
 * @param[out] settings H-pattern settings destination.
 */
static void h_pattern_calibration_decode(const uint8_t *data, HPatternSettings *settings) {
    settings->calibrated = data[0] == 1;
    settings->calibration.reverse_first_boundary = read_u16(data, 1);
    settings->calibration.first_third_boundary = read_u16(data, 3);
    settings->calibration.second_fourth_boundary = read_u16(data, 5);
    settings->calibration.third_fifth_boundary = read_u16(data, 7);
    settings->calibration.fourth_sixth_boundary = read_u16(data, 9);
    settings->calibration.fifth_seventh_boundary = read_u16(data, 11);
    settings->calibration.upper_row_threshold = read_u16(data, 13);
    settings->calibration.lower_row_threshold = read_u16(data, 15);
}

/**
 * @brief Encodes retained H-pattern shifter calibration.
 *
 * Stores the availability flag and eight axis thresholds in the compact payload.
 *
 * @param[in] settings H-pattern settings to encode.
 * @param[out] data Seventeen-byte calibration destination.
 */
static void h_pattern_calibration_encode(const HPatternSettings *settings, uint8_t *data) {
    data[0] = settings->calibrated ? 1 : 0;
    write_u16(data, 1, settings->calibration.reverse_first_boundary);
    write_u16(data, 3, settings->calibration.first_third_boundary);
    write_u16(data, 5, settings->calibration.second_fourth_boundary);
    write_u16(data, 7, settings->calibration.third_fifth_boundary);
    write_u16(data, 9, settings->calibration.fourth_sixth_boundary);
    write_u16(data, 11, settings->calibration.fifth_seventh_boundary);
    write_u16(data, 13, settings->calibration.upper_row_threshold);
    write_u16(data, 15, settings->calibration.lower_row_threshold);
}

/**
 * @brief Decodes retained auxiliary-axis endpoint settings.
 *
 * Restores the minimum, maximum, and startup-learning flag from the compact settings payload.
 *
 * @param[in] data Five-byte encoded auxiliary settings.
 * @param[out] settings Auxiliary settings destination.
 * @return True when the startup-learning flag is valid.
 */
static bool auxiliary_axis_settings_decode(const uint8_t *data, AuxiliaryAxisSettings *settings) {
    if (data[4] > 1) {
        return false;
    }
    settings->minimum = read_u16(data, 0);
    settings->maximum = read_u16(data, 2);
    settings->reset_on_start = data[4] == 1;
    return true;
}

/**
 * @brief Encodes retained auxiliary-axis endpoint settings.
 *
 * Stores the minimum, maximum, and startup-learning flag in the compact settings payload.
 *
 * @param[in] settings Auxiliary settings to encode.
 * @param[out] data Five-byte encoded auxiliary settings.
 */
static void auxiliary_axis_settings_encode(const AuxiliaryAxisSettings *settings, uint8_t *data) {
    write_u16(data, 0, settings->minimum);
    write_u16(data, 2, settings->maximum);
    data[4] = settings->reset_on_start ? 1 : 0;
}

/**
 * @brief Decodes retained steering-limit percentages.
 *
 * Restores one percentage for each of the six tuning profiles and rejects values above one
 * hundred.
 *
 * @param[in] data Six-byte encoded steering-limit settings.
 * @param[out] limits Per-profile steering-limit destination.
 * @return True when every encoded percentage is in range.
 */
static bool steering_limits_decode(const uint8_t *data, WheelSteeringLimits *limits) {
    for (uint8_t profile = 0; profile < TUNING_PROFILE_SLOT_COUNT; profile++) {
        if (data[profile] > WHEEL_STEERING_LIMIT_DEFAULT_PERCENT) {
            return false;
        }
        limits->percent[profile] = data[profile];
    }
    return true;
}

/**
 * @brief Encodes retained steering-limit percentages.
 *
 * Stores one percentage for each of the six tuning profiles.
 *
 * @param[in] limits Per-profile steering-limit settings.
 * @param[out] data Six-byte encoded steering-limit destination.
 */
static void steering_limits_encode(const WheelSteeringLimits *limits, uint8_t *data) {
    for (uint8_t profile = 0; profile < TUNING_PROFILE_SLOT_COUNT; profile++) {
        data[profile] = limits->percent[profile];
    }
}

/**
 * @brief Decodes one retained base-settings record.
 *
 * Validates the header and checksum, restores every setting available in the stored version, and
 * supplies defaults for settings introduced by later versions.
 *
 * @param[in] data Encoded retained-settings record.
 * @param[out] stored Decoded settings, generation, validity, and upgrade state.
 * @return True when the complete stored record is valid.
 */
static bool record_decode(const uint8_t data[PERSISTENCE_RECORD_SIZE], StoredSettings *stored) {
    uint16_t payload_size = read_u16(data, 6);
    uint16_t data_size = PERSISTENCE_HEADER_SIZE + payload_size;
    if (!header_valid(data, payload_size) ||
        read_u16(data, data_size) != persistence_checksum(data, data_size) ||
        !tuning_profile_record_decode(&data[PERSISTENCE_HEADER_SIZE],
                                      &stored->settings.tuning_profiles)) {
        stored->valid = false;
        return false;
    }

    stored->generation = read_u32(data, 8);
    uint8_t version = data[4];
    stored->needs_upgrade = version != PERSISTENCE_VERSION;
    if (version == PERSISTENCE_PROFILE_ONLY_VERSION) {
        wheel_position_reference_reset(&stored->settings.wheel_position);
    } else {
        uint16_t reference_offset = PERSISTENCE_HEADER_SIZE + TUNING_PROFILE_RECORD_SIZE;
        if (data[reference_offset] > 1) {
            stored->valid = false;
            return false;
        }
        stored->settings.wheel_position.calibrated = data[reference_offset] == 1;
        stored->settings.wheel_position.center = (int32_t)read_u32(data, reference_offset + 1);
    }
    if (version >= PERSISTENCE_H_PATTERN_VERSION) {
        uint16_t calibration_offset = PERSISTENCE_HEADER_SIZE + PERSISTENCE_REFERENCE_PAYLOAD_SIZE;
        if (data[calibration_offset] > 1) {
            stored->valid = false;
            return false;
        }
        h_pattern_calibration_decode(&data[calibration_offset],
                                     &stored->settings.h_pattern_shifter);
    } else {
        stored->settings.h_pattern_shifter = (HPatternSettings){0};
    }
    if (version >= PERSISTENCE_AUXILIARY_AXIS_VERSION) {
        uint16_t auxiliary_offset = PERSISTENCE_HEADER_SIZE + PERSISTENCE_H_PATTERN_PAYLOAD_SIZE;
        if (!auxiliary_axis_settings_decode(&data[auxiliary_offset],
                                            &stored->settings.auxiliary_axis)) {
            stored->valid = false;
            return false;
        }
    } else {
        auxiliary_axis_settings_defaults(&stored->settings.auxiliary_axis);
    }
    if (version >= PERSISTENCE_STEERING_LIMIT_VERSION) {
        uint16_t steering_limit_offset =
            PERSISTENCE_HEADER_SIZE + PERSISTENCE_AUXILIARY_AXIS_PAYLOAD_SIZE;
        if (!steering_limits_decode(&data[steering_limit_offset],
                                    &stored->settings.steering_limits)) {
            stored->valid = false;
            return false;
        }
    } else {
        wheel_steering_limits_defaults(&stored->settings.steering_limits);
    }
    if (version >= PERSISTENCE_AUXILIARY_OUTPUT_VERSION) {
        uint16_t auxiliary_output_offset =
            PERSISTENCE_HEADER_SIZE + PERSISTENCE_STEERING_LIMIT_PAYLOAD_SIZE;
        if (data[auxiliary_output_offset] > 1) {
            stored->valid = false;
            return false;
        }
        stored->settings.wheel_auxiliary_disabled = data[auxiliary_output_offset] == 1;
    } else {
        stored->settings.wheel_auxiliary_disabled = false;
    }
    if (version == PERSISTENCE_VERSION) {
        uint16_t security_code_offset =
            PERSISTENCE_HEADER_SIZE + PERSISTENCE_AUXILIARY_OUTPUT_PAYLOAD_SIZE;
        bool enabled = data[security_code_offset] == 1;
        bool disabled = data[security_code_offset] == 0;
        bool digits_valid = true;
        for (uint8_t digit = 0; digit < SECURITY_CODE_DIGIT_COUNT; digit++) {
            uint8_t value = data[security_code_offset + 1u + digit];
            digits_valid = digits_valid && value <= 9 && (enabled || value == 0);
            stored->settings.security_code.digits[digit] = value;
        }
        if ((!enabled && !disabled) || !digits_valid) {
            stored->valid = false;
            return false;
        }
        stored->settings.security_code.enabled = enabled;
    } else {
        stored->settings.security_code = (SecurityCodeSettings){0};
    }
    stored->valid = true;
    return true;
}

/**
 * @brief Reads and decodes one retained-settings slot.
 *
 * Marks the destination invalid when the platform read fails and otherwise validates the complete
 * record.
 *
 * @param[in] slot Retained-settings slot to read.
 * @param[out] stored Decoded settings and record metadata.
 * @return True when the slot contains a valid supported record.
 */
static bool record_read(PlatformStorageSlot slot, StoredSettings *stored) {
    uint8_t data[PERSISTENCE_RECORD_SIZE];
    if (!platform_storage_read(slot, data, PERSISTENCE_RECORD_SIZE)) {
        stored->valid = false;
        return false;
    }
    return record_decode(data, stored);
}

/**
 * @brief Encodes one complete retained base-settings record.
 *
 * Writes the current version header, generation, tuning profiles, calibration data, auxiliary
 * endpoints, per-profile steering limits, attached-wheel auxiliary option, security code, and final
 * checksum.
 *
 * @param[in] settings Base settings to retain.
 * @param[in] generation Monotonic record generation.
 * @param[out] data Encoded retained-settings destination.
 * @return True when the tuning profiles and complete record were encoded.
 */
static bool record_encode(const BaseSettings *settings, uint32_t generation,
                          uint8_t data[PERSISTENCE_RECORD_SIZE]) {
    data[0] = PERSISTENCE_MAGIC_0;
    data[1] = PERSISTENCE_MAGIC_1;
    data[2] = PERSISTENCE_MAGIC_2;
    data[3] = PERSISTENCE_MAGIC_3;
    data[4] = PERSISTENCE_VERSION;
    data[5] = 0;
    write_u16(data, 6, PERSISTENCE_PAYLOAD_SIZE);
    write_u32(data, 8, generation);
    if (!tuning_profile_record_encode(&settings->tuning_profiles, &data[PERSISTENCE_HEADER_SIZE])) {
        return false;
    }
    uint16_t reference_offset = PERSISTENCE_HEADER_SIZE + TUNING_PROFILE_RECORD_SIZE;
    data[reference_offset] = settings->wheel_position.calibrated ? 1 : 0;
    write_u32(data, reference_offset + 1, (uint32_t)settings->wheel_position.center);
    uint16_t calibration_offset = PERSISTENCE_HEADER_SIZE + PERSISTENCE_REFERENCE_PAYLOAD_SIZE;
    h_pattern_calibration_encode(&settings->h_pattern_shifter, &data[calibration_offset]);
    uint16_t auxiliary_offset = PERSISTENCE_HEADER_SIZE + PERSISTENCE_H_PATTERN_PAYLOAD_SIZE;
    auxiliary_axis_settings_encode(&settings->auxiliary_axis, &data[auxiliary_offset]);
    uint16_t steering_limit_offset =
        PERSISTENCE_HEADER_SIZE + PERSISTENCE_AUXILIARY_AXIS_PAYLOAD_SIZE;
    steering_limits_encode(&settings->steering_limits, &data[steering_limit_offset]);
    uint16_t auxiliary_output_offset =
        PERSISTENCE_HEADER_SIZE + PERSISTENCE_STEERING_LIMIT_PAYLOAD_SIZE;
    data[auxiliary_output_offset] = settings->wheel_auxiliary_disabled ? 1 : 0;
    uint16_t security_code_offset =
        PERSISTENCE_HEADER_SIZE + PERSISTENCE_AUXILIARY_OUTPUT_PAYLOAD_SIZE;
    data[security_code_offset] = settings->security_code.enabled ? 1 : 0;
    for (uint8_t digit = 0; digit < SECURITY_CODE_DIGIT_COUNT; digit++) {
        uint8_t value = settings->security_code.digits[digit];
        if (value > 9) {
            return false;
        }
        data[security_code_offset + 1u + digit] = settings->security_code.enabled ? value : 0;
    }
    write_u16(data, PERSISTENCE_DATA_SIZE, persistence_checksum(data, PERSISTENCE_DATA_SIZE));
    return true;
}

/**
 * @brief Confirms a complete retained-settings write.
 *
 * Reads the selected slot and compares every encoded byte with the requested record.
 *
 * @param[in] slot Retained-settings slot to inspect.
 * @param[in] expected Record expected in the slot.
 * @return True when the complete slot prefix matches the expected record.
 */
static bool record_matches(PlatformStorageSlot slot,
                           const uint8_t expected[PERSISTENCE_RECORD_SIZE]) {
    uint8_t actual[PERSISTENCE_RECORD_SIZE];
    if (!platform_storage_read(slot, actual, PERSISTENCE_RECORD_SIZE)) {
        return false;
    }
    for (uint16_t index = 0; index < PERSISTENCE_RECORD_SIZE; index++) {
        if (actual[index] != expected[index]) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Restores the newest valid retained base settings.
 *
 * Selects the newer of two redundant records, supplies defaults when neither record is valid, and
 * regenerates setup 1 instead of trusting its retained values.
 *
 * @param[out] persistence Retained-settings service state.
 * @param[out] settings Restored or default base settings.
 * @return True when a valid retained record was restored.
 */
bool base_settings_persistence_load(BaseSettingsPersistence *persistence, BaseSettings *settings) {
    StoredSettings records[PLATFORM_STORAGE_SLOT_COUNT];
    record_read(PLATFORM_STORAGE_SETTINGS_A, &records[PLATFORM_STORAGE_SETTINGS_A]);
    record_read(PLATFORM_STORAGE_SETTINGS_B, &records[PLATFORM_STORAGE_SETTINGS_B]);

    uint8_t selected = PLATFORM_STORAGE_SETTINGS_A;
    if (records[PLATFORM_STORAGE_SETTINGS_B].valid &&
        (!records[PLATFORM_STORAGE_SETTINGS_A].valid ||
         generation_is_newer(records[PLATFORM_STORAGE_SETTINGS_B].generation,
                             records[PLATFORM_STORAGE_SETTINGS_A].generation))) {
        selected = PLATFORM_STORAGE_SETTINGS_B;
    }

    if (records[selected].valid) {
        *settings = records[selected].settings;
        tuning_profile_defaults(&settings->tuning_profiles.slots[0]);
        persistence->generation = records[selected].generation;
        persistence->active_slot = selected;
        persistence->has_record = true;
        persistence->dirty = records[selected].needs_upgrade;
        return true;
    }

    base_settings_defaults(settings);
    persistence->generation = 0;
    persistence->active_slot = PLATFORM_STORAGE_SETTINGS_A;
    persistence->has_record = false;
    persistence->dirty = true;
    return false;
}

/**
 * @brief Marks retained settings as changed.
 *
 * Defers storage until an explicit save boundary.
 *
 * @param[in,out] persistence Retained-settings state to mark.
 */
void base_settings_persistence_mark_dirty(BaseSettingsPersistence *persistence) {
    persistence->dirty = true;
}

/**
 * @brief Saves changed base settings.
 *
 * Writes the next generation to the inactive slot, confirms the complete record, and preserves the
 * previous valid slot when encoding or replacement fails. Clean settings do not consume a write.
 *
 * @param[in,out] persistence Retained-settings state.
 * @param[in] settings Current base settings.
 * @return Saved, retry, or idle according to the write result and dirty state.
 */
BaseSettingsPersistenceResult base_settings_persistence_save(BaseSettingsPersistence *persistence,
                                                             const BaseSettings *settings) {
    if (!persistence->dirty) {
        return BASE_SETTINGS_PERSISTENCE_IDLE;
    }

    uint8_t target = persistence->has_record ? (uint8_t)(persistence->active_slot ^ 1U)
                                             : PLATFORM_STORAGE_SETTINGS_A;
    uint32_t generation = persistence->generation + 1;
    uint8_t data[PERSISTENCE_RECORD_SIZE];
    if (!record_encode(settings, generation, data) ||
        !platform_storage_replace((PlatformStorageSlot)target, data, PERSISTENCE_RECORD_SIZE) ||
        !record_matches((PlatformStorageSlot)target, data)) {
        return BASE_SETTINGS_PERSISTENCE_RETRY;
    }

    persistence->generation = generation;
    persistence->active_slot = target;
    persistence->has_record = true;
    persistence->dirty = false;
    return BASE_SETTINGS_PERSISTENCE_SAVED;
}
