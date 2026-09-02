#include "settings/persistence.h"

#include <stdbool.h>
#include <stdint.h>

#include "platform/storage.h"
#include "profile/tuning.h"
#include "settings/state.h"
#include "usb/tuning_profile_report.h"
#include "wheel/position.h"

/** @brief Reference-compatible indexes and markers for retained settings. */
enum {
    SETTINGS_FORMAT_INDEX = 0,              /**< Settings format value index. */
    SETTINGS_FORMAT_VALUE = 0x0300,         /**< Supported settings format marker. */
    WHEEL_CENTER_LOW_INDEX = 1,             /**< Low word of the wheel center. */
    WHEEL_CENTER_HIGH_INDEX = 2,            /**< High word of the wheel center. */
    GLOBAL_FIRST_INDEX = 3,                 /**< First global compatibility value. */
    GLOBAL_VALUE_COUNT = 2,                 /**< Number of global compatibility values. */
    STANDARD_MODE_INDEX = 5,                /**< Standard-mode value index. */
    SELECTED_PROFILE_INDEX = 6,             /**< One-based selected-profile index. */
    H_PATTERN_FIRST_INDEX = 7,              /**< First H-pattern calibration index. */
    H_PATTERN_VALUE_COUNT = 9,              /**< Number of H-pattern retained values. */
    H_PATTERN_VALID_INDEX = 17,             /**< H-pattern validity index. */
    SECURITY_CODE_INDEX = 18,               /**< Security-code value index. */
    OPERATING_MODE_INDEX = 19,              /**< Operating-mode value index. */
    OPERATING_MODE_PREFIX = 0xaa00,         /**< Operating-mode validity prefix. */
    STEERING_LIMIT_FIRST_INDEX = 20,        /**< First steering-limit index. */
    STEERING_LIMIT_PREFIX = 0xaa00,         /**< Steering-limit validity prefix. */
    WHEEL_AUXILIARY_OPTION_INDEX = 26,      /**< Wheel auxiliary-option index. */
    WHEEL_AUXILIARY_OPTION_PREFIX = 0xaa00, /**< Wheel auxiliary-option marker. */
    AUXILIARY_MINIMUM_INDEX = 27,           /**< Auxiliary minimum index. */
    AUXILIARY_MAXIMUM_INDEX = 28,           /**< Auxiliary maximum index. */
    AUXILIARY_RESET_INDEX = 29,             /**< Auxiliary reset flag index. */
    PROFILE_FIRST_INDEX = 30,               /**< First retained profile index. */
    PROFILE_STORAGE_STRIDE = 26,            /**< Retained values per profile block. */
};

/**
 * @brief Reads a retained setting when present.
 *
 * Leaves the destination unchanged when the indexed journal has no value.
 *
 * @param[in] index Reference-compatible settings index.
 * @param[out] value Destination receiving the retained value when present.
 * @return True when the index has a retained value; otherwise false.
 */
static bool value_read(uint16_t index, uint16_t *value) {
    uint16_t stored;
    if (!platform_storage_value_read(index, &stored)) {
        return false;
    }
    *value = stored;
    return true;
}

/**
 * @brief Writes the low and high words of a signed 32-bit value.
 *
 * Appends both words at consecutive reference-compatible settings indices.
 *
 * @param[in] first_index Index receiving the low word.
 * @param[in] value Signed value to retain.
 * @return True when both words are retained; otherwise false.
 */
static bool value_write_i32(uint16_t first_index, int32_t value) {
    uint32_t encoded = (uint32_t)value;
    return platform_storage_value_write(first_index, (uint16_t)encoded) &&
           platform_storage_value_write(first_index + 1, (uint16_t)(encoded >> 16));
}

/**
 * @brief Restores the retained wheel-center reference.
 *
 * Combines settings indices one and two into the signed center value and follows the reference
 * behavior of treating an all-zero center as uncalibrated.
 *
 * @param[in,out] settings Base settings receiving the wheel reference.
 */
static void wheel_reference_load(BaseSettings *settings) {
    uint16_t low;
    uint16_t high;
    if (value_read(WHEEL_CENTER_LOW_INDEX, &low) && value_read(WHEEL_CENTER_HIGH_INDEX, &high)) {
        settings->wheel_position.center = (int32_t)((uint32_t)low | (uint32_t)high << 16);
        settings->wheel_position.calibrated = settings->wheel_position.center != 0;
    }
}

/**
 * @brief Restores Standard-mode and selected-profile state.
 *
 * Reads the reference Boolean mode at index five and converts the one-based profile at index six
 * to the logical zero-based setup bank.
 *
 * @param[in,out] settings Base settings receiving tuning-bank state.
 */
static void tuning_bank_state_load(BaseSettings *settings) {
    uint16_t value;
    if (value_read(STANDARD_MODE_INDEX, &value) && value <= 1) {
        settings->tuning_profiles.standard_mode_enabled = value != 0;
    }
    if (value_read(SELECTED_PROFILE_INDEX, &value) && value >= 1 &&
        value <= TUNING_PROFILE_SLOT_COUNT) {
        settings->tuning_profiles.selected_slot = (uint8_t)(value - 1);
        settings->tuning_profiles.active_slot = settings->tuning_profiles.selected_slot;
    }
}

/**
 * @brief Restores six retained tuning profiles.
 *
 * Reads the first 25 low-byte values from each 26-index profile block and decodes the same logical
 * values used by the device-control tuning report. The unimplemented final value in each reference
 * block is retained separately for round-trip persistence.
 *
 * @param[in,out] settings Base settings receiving tuning profiles.
 */
static void tuning_profiles_load(BaseSettings *settings) {
    for (uint8_t profile = 0; profile < TUNING_PROFILE_SLOT_COUNT; profile++) {
        uint8_t encoded[USB_TUNING_PROFILE_VALUE_COUNT];
        bool complete = true;
        uint16_t first = PROFILE_FIRST_INDEX + (uint16_t)profile * PROFILE_STORAGE_STRIDE;
        for (uint8_t field = 0; field < USB_TUNING_PROFILE_VALUE_COUNT; field++) {
            uint16_t value;
            if (!value_read(first + field, &value)) {
                complete = false;
                break;
            }
            encoded[field] = (uint8_t)value;
        }
        if (complete) {
            usb_tuning_profile_report_decode(encoded, &settings->tuning_profiles.slots[profile]);
        }
        value_read(first + USB_TUNING_PROFILE_VALUE_COUNT,
                   &settings->retained_profile_values[profile]);
    }
    tuning_profile_defaults(&settings->tuning_profiles.slots[0]);
}

/**
 * @brief Restores H-pattern calibration values.
 *
 * Accepts the nine consecutive thresholds only when reference validity index 17 contains one.
 *
 * @param[in,out] settings Base settings receiving shifter calibration.
 */
static void h_pattern_load(BaseSettings *settings) {
    uint16_t valid;
    if (!value_read(H_PATTERN_VALID_INDEX, &valid) || valid != 1) {
        return;
    }
    uint16_t *thresholds = &settings->h_pattern_shifter.calibration.reverse_first_boundary;
    for (uint8_t index = 0; index < H_PATTERN_VALUE_COUNT; index++) {
        if (!value_read(H_PATTERN_FIRST_INDEX + index, &thresholds[index])) {
            return;
        }
    }
    settings->h_pattern_shifter.calibrated = true;
}

/**
 * @brief Restores the three-digit security code.
 *
 * Recognizes the A marker nibble and extracts the three decimal digits from consecutive nibbles.
 * Invalid or absent marker values are replaced with the disabled value in persistent storage.
 *
 * @param[in,out] settings Base settings receiving security-code state.
 */
static void security_code_load(BaseSettings *settings) {
    uint16_t encoded;
    if (!value_read(SECURITY_CODE_INDEX, &encoded) ||
        (encoded & UINT16_C(0xf000)) != UINT16_C(0xa000)) {
        (void)platform_storage_value_write(SECURITY_CODE_INDEX, 0);
        return;
    }
    for (uint8_t digit = 0; digit < SECURITY_CODE_DIGIT_COUNT; digit++) {
        uint8_t value = (uint8_t)(encoded >> (digit * 4)) & 0x0f;
        if (value > 9) {
            return;
        }
        settings->security_code.digits[digit] = value;
    }
    settings->security_code.enabled = true;
}

/**
 * @brief Restores per-profile steering limits.
 *
 * Accepts each setting only when its high byte contains the AA marker and its low byte is a valid
 * percentage.
 *
 * @param[in,out] settings Base settings receiving steering limits.
 */
static void steering_limits_load(BaseSettings *settings) {
    for (uint8_t profile = 0; profile < TUNING_PROFILE_SLOT_COUNT; profile++) {
        uint16_t encoded;
        if (value_read(STEERING_LIMIT_FIRST_INDEX + profile, &encoded) &&
            (encoded & UINT16_C(0xff00)) == STEERING_LIMIT_PREFIX &&
            (uint8_t)encoded <= WHEEL_STEERING_LIMIT_DEFAULT_PERCENT) {
            settings->steering_limits.percent[profile] = (uint8_t)encoded;
        }
    }
}

/**
 * @brief Restores auxiliary-axis and attached-wheel output settings.
 *
 * Reads the attached-wheel option at index 26 and the auxiliary calibration at indices 27 through
 * 29. A marked attached-wheel option is consumed at startup and replaced by marked option zero;
 * an unmarked value supplies its low byte directly.
 *
 * @param[in,out] settings Base settings receiving extended retained values.
 */
static void extended_settings_load(BaseSettings *settings) {
    uint16_t minimum;
    uint16_t maximum;
    uint16_t flag;
    if (value_read(AUXILIARY_MINIMUM_INDEX, &minimum) &&
        value_read(AUXILIARY_MAXIMUM_INDEX, &maximum) && minimum < maximum) {
        settings->auxiliary_axis.minimum = minimum;
        settings->auxiliary_axis.maximum = maximum;
    }
    if (value_read(AUXILIARY_RESET_INDEX, &flag) && flag <= 1) {
        settings->auxiliary_axis.reset_on_start = flag != 0;
    }
    if (value_read(WHEEL_AUXILIARY_OPTION_INDEX, &flag)) {
        if ((flag & UINT16_C(0xff00)) == WHEEL_AUXILIARY_OPTION_PREFIX) {
            settings->wheel_auxiliary_option = 0;
            platform_storage_value_write(WHEEL_AUXILIARY_OPTION_INDEX,
                                         WHEEL_AUXILIARY_OPTION_PREFIX);
        } else {
            settings->wheel_auxiliary_option = (uint8_t)flag;
        }
    }
}

/**
 * @brief Loads retained compatibility values and operating mode.
 *
 * Restores the two global compatibility words and accepts operating-mode index 19 only when its
 * stored high byte contains the expected validity prefix.
 *
 * @param[in,out] settings Base settings receiving retained compatibility state.
 */
static void compatibility_settings_load(BaseSettings *settings) {
    for (uint8_t index = 0; index < GLOBAL_VALUE_COUNT; index++) {
        value_read(GLOBAL_FIRST_INDEX + index, &settings->retained_global_values[index]);
    }
    uint16_t operating_mode;
    if (value_read(OPERATING_MODE_INDEX, &operating_mode) &&
        (operating_mode & UINT16_C(0xff00)) == OPERATING_MODE_PREFIX) {
        settings->operating_mode = (uint8_t)operating_mode;
        settings->operating_mode_valid = true;
    }
}

/**
 * @brief Writes six logical tuning profiles.
 *
 * Encodes each profile in device-control order and writes the first 25 values of its 26-index
 * reference block and re-emits the retained unknown final index.
 *
 * @param[in] settings Base settings containing the profiles.
 * @return True when every implemented profile value is retained; otherwise false.
 */
static bool tuning_profiles_save(const BaseSettings *settings) {
    for (uint8_t profile = 0; profile < TUNING_PROFILE_SLOT_COUNT; profile++) {
        uint8_t encoded[USB_TUNING_PROFILE_VALUE_COUNT];
        usb_tuning_profile_report_encode(&settings->tuning_profiles.slots[profile], encoded);
        uint16_t first = PROFILE_FIRST_INDEX + (uint16_t)profile * PROFILE_STORAGE_STRIDE;
        for (uint8_t field = 0; field < USB_TUNING_PROFILE_VALUE_COUNT; field++) {
            if (!platform_storage_value_write(first + field, encoded[field])) {
                return false;
            }
        }
        if (!platform_storage_value_write(first + USB_TUNING_PROFILE_VALUE_COUNT,
                                          settings->retained_profile_values[profile])) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Writes H-pattern calibration values and validity.
 *
 * Writes all nine thresholds before publishing validity index 17 so incomplete first-time writes
 * are not accepted as calibrated.
 *
 * @param[in] settings Base settings containing shifter calibration.
 * @return True when the thresholds and validity state are retained; otherwise false.
 */
static bool h_pattern_save(const BaseSettings *settings) {
    const uint16_t *thresholds = &settings->h_pattern_shifter.calibration.reverse_first_boundary;
    for (uint8_t index = 0; index < H_PATTERN_VALUE_COUNT; index++) {
        if (!platform_storage_value_write(H_PATTERN_FIRST_INDEX + index, thresholds[index])) {
            return false;
        }
    }
    return platform_storage_value_write(H_PATTERN_VALID_INDEX,
                                        settings->h_pattern_shifter.calibrated ? 1 : 0);
}

/**
 * @brief Encodes and writes the three-digit security code.
 *
 * Writes zero when security is disabled or the A marker followed by three decimal nibbles when it
 * is enabled.
 *
 * @param[in] settings Base settings containing security-code state.
 * @return True when the security setting is valid and retained; otherwise false.
 */
static bool security_code_save(const BaseSettings *settings) {
    uint16_t encoded = 0;
    if (settings->security_code.enabled) {
        encoded = UINT16_C(0xa000);
        for (uint8_t digit = 0; digit < SECURITY_CODE_DIGIT_COUNT; digit++) {
            uint8_t value = settings->security_code.digits[digit];
            if (value > 9) {
                return false;
            }
            encoded |= (uint16_t)value << (digit * 4);
        }
    }
    return platform_storage_value_write(SECURITY_CODE_INDEX, encoded);
}

/**
 * @brief Writes per-profile steering limits.
 *
 * Combines the AA validity marker with each zero-through-100 percentage at indices 20 through 25.
 *
 * @param[in] settings Base settings containing steering limits.
 * @return True when all six marked limits are retained; otherwise false.
 */
static bool steering_limits_save(const BaseSettings *settings) {
    for (uint8_t profile = 0; profile < TUNING_PROFILE_SLOT_COUNT; profile++) {
        uint8_t percent = settings->steering_limits.percent[profile];
        if (percent > WHEEL_STEERING_LIMIT_DEFAULT_PERCENT ||
            !platform_storage_value_write(STEERING_LIMIT_FIRST_INDEX + profile,
                                          STEERING_LIMIT_PREFIX | percent)) {
            return false;
        }
    }
    return true;
}

bool base_settings_persistence_load(BaseSettingsPersistence *persistence, BaseSettings *settings) {
    base_settings_defaults(settings);
    persistence->has_record = false;
    persistence->dirty = true;
    uint16_t format;
    if (!platform_storage_initialize() || !value_read(SETTINGS_FORMAT_INDEX, &format) ||
        format != SETTINGS_FORMAT_VALUE) {
        return false;
    }

    wheel_reference_load(settings);
    tuning_bank_state_load(settings);
    tuning_profiles_load(settings);
    h_pattern_load(settings);
    security_code_load(settings);
    steering_limits_load(settings);
    extended_settings_load(settings);
    compatibility_settings_load(settings);
    persistence->has_record = true;
    persistence->dirty = false;
    return true;
}

void base_settings_persistence_mark_dirty(BaseSettingsPersistence *persistence) {
    persistence->dirty = true;
}

BaseSettingsPersistenceResult base_settings_persistence_save(BaseSettingsPersistence *persistence,
                                                             const BaseSettings *settings) {
    if (!persistence->dirty) {
        return BASE_SETTINGS_PERSISTENCE_IDLE;
    }

    int32_t center = settings->wheel_position.calibrated ? settings->wheel_position.center : 0;
    bool successful =
        value_write_i32(WHEEL_CENTER_LOW_INDEX, center) &&
        platform_storage_value_write(GLOBAL_FIRST_INDEX, settings->retained_global_values[0]) &&
        platform_storage_value_write(GLOBAL_FIRST_INDEX + 1, settings->retained_global_values[1]) &&
        platform_storage_value_write(STANDARD_MODE_INDEX,
                                     settings->tuning_profiles.standard_mode_enabled ? 1 : 0) &&
        settings->tuning_profiles.selected_slot < TUNING_PROFILE_SLOT_COUNT &&
        platform_storage_value_write(SELECTED_PROFILE_INDEX,
                                     settings->tuning_profiles.selected_slot + 1) &&
        h_pattern_save(settings) && security_code_save(settings) &&
        platform_storage_value_write(OPERATING_MODE_INDEX,
                                     OPERATING_MODE_PREFIX | settings->operating_mode) &&
        steering_limits_save(settings) && tuning_profiles_save(settings) &&
        platform_storage_value_write(AUXILIARY_MINIMUM_INDEX, settings->auxiliary_axis.minimum) &&
        platform_storage_value_write(AUXILIARY_MAXIMUM_INDEX, settings->auxiliary_axis.maximum) &&
        platform_storage_value_write(AUXILIARY_RESET_INDEX,
                                     settings->auxiliary_axis.reset_on_start ? 1 : 0) &&
        platform_storage_value_write(WHEEL_AUXILIARY_OPTION_INDEX,
                                     WHEEL_AUXILIARY_OPTION_PREFIX |
                                         settings->wheel_auxiliary_option) &&
        platform_storage_value_write(SETTINGS_FORMAT_INDEX, SETTINGS_FORMAT_VALUE);
    if (!successful) {
        return BASE_SETTINGS_PERSISTENCE_RETRY;
    }

    persistence->has_record = true;
    persistence->dirty = false;
    return BASE_SETTINGS_PERSISTENCE_SAVED;
}
