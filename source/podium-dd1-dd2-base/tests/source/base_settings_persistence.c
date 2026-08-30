#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform/storage.h"
#include "profile/record.h"
#include "settings/persistence.h"
#include "wheel/position.h"

static uint8_t storage[PLATFORM_STORAGE_SLOT_COUNT][PLATFORM_STORAGE_SLOT_SIZE];
static uint16_t replace_count;
static bool replace_fails;

static uint16_t checksum(const uint8_t *data, uint16_t size) {
    uint16_t value = UINT16_C(0xffff);
    for (uint16_t index = 0; index < size; index++) {
        value ^= (uint16_t)data[index] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            value = (value & UINT16_C(0x8000)) != 0 ? (uint16_t)((value << 1) ^ UINT16_C(0x1021))
                                                    : (uint16_t)(value << 1);
        }
    }
    return value;
}

static void write_u16(uint8_t *data, uint16_t offset, uint16_t value) {
    data[offset] = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
}

static void write_u32(uint8_t *data, uint16_t offset, uint32_t value) {
    data[offset] = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
    data[offset + 2] = (uint8_t)(value >> 16);
    data[offset + 3] = (uint8_t)(value >> 24);
}

static void set_generation(PlatformStorageSlot slot, uint32_t generation) {
    enum { HEADER_SIZE = 12, GENERATION_OFFSET = 8 };
    uint8_t *record = storage[slot];
    uint16_t payload_size = (uint16_t)record[6] | (uint16_t)record[7] << 8;
    uint16_t data_size = HEADER_SIZE + payload_size;
    write_u32(record, GENERATION_OFFSET, generation);
    write_u16(record, data_size, checksum(record, data_size));
}

bool platform_storage_read(PlatformStorageSlot slot, uint8_t *data, uint16_t size) {
    if (slot >= PLATFORM_STORAGE_SLOT_COUNT || size > PLATFORM_STORAGE_SLOT_SIZE) {
        return false;
    }
    memcpy(data, storage[slot], size);
    return true;
}

bool platform_storage_replace(PlatformStorageSlot slot, const uint8_t *data, uint16_t size) {
    if (slot >= PLATFORM_STORAGE_SLOT_COUNT || size > PLATFORM_STORAGE_SLOT_SIZE) {
        return false;
    }
    replace_count++;
    memset(storage[slot], UINT8_MAX, sizeof(storage[slot]));
    if (replace_fails) {
        storage[slot][0] = 0;
        return false;
    }
    memcpy(storage[slot], data, size);
    return true;
}

static void reset_storage(void) {
    memset(storage, UINT8_MAX, sizeof(storage));
    replace_count = 0;
    replace_fails = false;
}

static void test_defaults_are_saved_on_initialization(void) {
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    assert(!base_settings_persistence_load(&persistence, &settings));
    assert(persistence.dirty);
    assert(settings.tuning_profiles.selected_slot == 0);
    assert(!settings.wheel_position.calibrated);
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);
    assert(replace_count == 1);
    assert(!persistence.dirty);

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored));
    assert(restored.tuning_profiles.slots[0].rotation_degrees ==
           settings.tuning_profiles.slots[0].rotation_degrees);
    assert(!restored.wheel_position.calibrated);
    assert(restored.auxiliary_axis.minimum == 0x0f38);
    assert(restored.auxiliary_axis.maximum == 0x00c8);
    assert(restored.auxiliary_axis.reset_on_start);
    assert(wheel_steering_limits_active(&restored.steering_limits, 0) == 100);
    assert(!restored.wheel_auxiliary_disabled);
    assert(!restored.security_code.enabled);
    assert(restored.security_code.digits[0] == 0);
    assert(restored.security_code.digits[1] == 0);
    assert(restored.security_code.digits[2] == 0);
}

static void test_dirty_changes_wait_for_explicit_save(void) {
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings);
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    settings.tuning_profiles.slots[1].rotation_degrees = 720;
    base_settings_persistence_mark_dirty(&persistence);
    assert(persistence.dirty);
    assert(replace_count == 1);
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);
    assert(replace_count == 2);

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored));
    assert(restored.tuning_profiles.slots[1].rotation_degrees == 720);
}

static void test_standard_profile_is_regenerated(void) {
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings);
    settings.tuning_profiles.slots[0].force_feedback_strength = 80;
    settings.tuning_profiles.slots[1].force_feedback_strength = 70;
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored));
    assert(restored.tuning_profiles.slots[0].force_feedback_strength == 35);
    assert(restored.tuning_profiles.slots[1].force_feedback_strength == 70);
}

static void test_retained_selection_becomes_active(void) {
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings);
    settings.tuning_profiles.selected_slot = 4;
    settings.tuning_profiles.active_slot = 2;
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored));
    assert(restored.tuning_profiles.selected_slot == 4);
    assert(restored.tuning_profiles.active_slot == 4);
}

static void test_wheel_reference_is_persisted(void) {
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings);
    assert(wheel_position_reference_capture(&settings.wheel_position, 20000, UINT32_C(0x5d2b)));
    base_settings_persistence_mark_dirty(&persistence);
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored));
    assert(restored.wheel_position.calibrated);
    assert(restored.wheel_position.center == 20000);
}

static void test_shifter_calibration_is_persisted(void) {
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings);
    settings.h_pattern_shifter = (HPatternSettings){
        .calibration =
            {
                .reverse_first_boundary = 800,
                .first_third_boundary = 600,
                .second_fourth_boundary = 590,
                .third_fifth_boundary = 400,
                .fourth_sixth_boundary = 390,
                .fifth_seventh_boundary = 200,
                .upper_row_threshold = 700,
                .lower_row_threshold = 300,
            },
        .calibrated = true,
    };
    base_settings_persistence_mark_dirty(&persistence);
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored));
    assert(restored.h_pattern_shifter.calibrated);
    assert(restored.h_pattern_shifter.calibration.reverse_first_boundary == 800);
    assert(restored.h_pattern_shifter.calibration.first_third_boundary == 600);
    assert(restored.h_pattern_shifter.calibration.second_fourth_boundary == 590);
    assert(restored.h_pattern_shifter.calibration.third_fifth_boundary == 400);
    assert(restored.h_pattern_shifter.calibration.fourth_sixth_boundary == 390);
    assert(restored.h_pattern_shifter.calibration.fifth_seventh_boundary == 200);
    assert(restored.h_pattern_shifter.calibration.upper_row_threshold == 700);
    assert(restored.h_pattern_shifter.calibration.lower_row_threshold == 300);
}

static void test_auxiliary_axis_settings_are_persisted(void) {
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings);
    settings.auxiliary_axis = (AuxiliaryAxisSettings){
        .minimum = 240,
        .maximum = 3780,
        .reset_on_start = false,
    };
    base_settings_persistence_mark_dirty(&persistence);
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored));
    assert(restored.auxiliary_axis.minimum == 240);
    assert(restored.auxiliary_axis.maximum == 3780);
    assert(!restored.auxiliary_axis.reset_on_start);
}

static void test_steering_limit_settings_are_persisted(void) {
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings);
    settings.steering_limits.percent[0] = 35;
    settings.steering_limits.percent[5] = 80;
    base_settings_persistence_mark_dirty(&persistence);
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored));
    assert(restored.steering_limits.percent[0] == 35);
    assert(restored.steering_limits.percent[5] == 80);
    assert(restored.steering_limits.percent[1] == 100);
}

static void test_wheel_auxiliary_option_is_persisted(void) {
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings);
    settings.wheel_auxiliary_disabled = true;
    base_settings_persistence_mark_dirty(&persistence);
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored));
    assert(restored.wheel_auxiliary_disabled);
}

static void test_security_code_is_persisted(void) {
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings);
    settings.security_code = (SecurityCodeSettings){
        .digits = {4, 2, 7},
        .enabled = true,
    };
    base_settings_persistence_mark_dirty(&persistence);
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored));
    assert(restored.security_code.enabled);
    assert(restored.security_code.digits[0] == 4);
    assert(restored.security_code.digits[1] == 2);
    assert(restored.security_code.digits[2] == 7);
}

static void test_invalid_wheel_auxiliary_option_is_rejected(void) {
    enum { HEADER_SIZE = 12 };
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings);
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    uint8_t *record = storage[PLATFORM_STORAGE_SETTINGS_A];
    uint16_t payload_size = (uint16_t)record[6] | (uint16_t)record[7] << 8;
    uint16_t data_size = HEADER_SIZE + payload_size;
    record[data_size - 5] = 2;
    write_u16(record, data_size, checksum(record, data_size));

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(!base_settings_persistence_load(&loaded, &restored));
    assert(!restored.wheel_auxiliary_disabled);
}

static void test_invalid_security_code_is_rejected(void) {
    enum { HEADER_SIZE = 12, SECURITY_CODE_SIZE = 4 };
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings);
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    uint8_t *record = storage[PLATFORM_STORAGE_SETTINGS_A];
    uint16_t payload_size = (uint16_t)record[6] | (uint16_t)record[7] << 8;
    uint16_t data_size = HEADER_SIZE + payload_size;
    uint16_t security_code_offset = data_size - SECURITY_CODE_SIZE;
    record[security_code_offset] = 1;
    record[security_code_offset + 1] = 10;
    write_u16(record, data_size, checksum(record, data_size));

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(!base_settings_persistence_load(&loaded, &restored));
    assert(!restored.security_code.enabled);
}

static void test_profile_only_record_is_upgraded(void) {
    enum { HEADER_SIZE = 12, PROFILE_ONLY_VERSION = 1 };
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings);
    settings.tuning_profiles.slots[1].rotation_degrees = 720;
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    uint8_t *record = storage[PLATFORM_STORAGE_SETTINGS_A];
    uint16_t data_size = HEADER_SIZE + TUNING_PROFILE_RECORD_SIZE;
    record[4] = PROFILE_ONLY_VERSION;
    write_u16(record, 6, TUNING_PROFILE_RECORD_SIZE);
    write_u16(record, data_size, checksum(record, data_size));

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored));
    assert(loaded.dirty);
    assert(!restored.wheel_position.calibrated);
    assert(restored.tuning_profiles.slots[1].rotation_degrees == 720);
    assert(base_settings_persistence_save(&loaded, &restored) == BASE_SETTINGS_PERSISTENCE_SAVED);
}

static void test_wheel_reference_record_is_upgraded(void) {
    enum { HEADER_SIZE = 12, WHEEL_REFERENCE_VERSION = 2, WHEEL_REFERENCE_SIZE = 5 };
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings);
    assert(wheel_position_reference_capture(&settings.wheel_position, 12345, UINT32_C(0x5d2b)));
    settings.h_pattern_shifter.calibrated = true;
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    uint8_t *record = storage[PLATFORM_STORAGE_SETTINGS_A];
    uint16_t payload_size = TUNING_PROFILE_RECORD_SIZE + WHEEL_REFERENCE_SIZE;
    uint16_t data_size = HEADER_SIZE + payload_size;
    record[4] = WHEEL_REFERENCE_VERSION;
    write_u16(record, 6, payload_size);
    write_u16(record, data_size, checksum(record, data_size));

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored));
    assert(loaded.dirty);
    assert(restored.wheel_position.calibrated);
    assert(restored.wheel_position.center == 12345);
    assert(!restored.h_pattern_shifter.calibrated);
}

static void test_h_pattern_record_is_upgraded(void) {
    enum {
        HEADER_SIZE = 12,
        H_PATTERN_VERSION = 3,
        WHEEL_REFERENCE_SIZE = 5,
        H_PATTERN_SIZE = 17,
    };
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings);
    settings.h_pattern_shifter.calibrated = true;
    settings.auxiliary_axis = (AuxiliaryAxisSettings){
        .minimum = 240,
        .maximum = 3780,
        .reset_on_start = false,
    };
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    uint8_t *record = storage[PLATFORM_STORAGE_SETTINGS_A];
    uint16_t payload_size = TUNING_PROFILE_RECORD_SIZE + WHEEL_REFERENCE_SIZE + H_PATTERN_SIZE;
    uint16_t data_size = HEADER_SIZE + payload_size;
    record[4] = H_PATTERN_VERSION;
    write_u16(record, 6, payload_size);
    write_u16(record, data_size, checksum(record, data_size));

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored));
    assert(loaded.dirty);
    assert(restored.h_pattern_shifter.calibrated);
    assert(restored.auxiliary_axis.minimum == 0x0f38);
    assert(restored.auxiliary_axis.maximum == 0x00c8);
    assert(restored.auxiliary_axis.reset_on_start);
}

static void test_auxiliary_axis_record_is_upgraded(void) {
    enum {
        HEADER_SIZE = 12,
        AUXILIARY_AXIS_VERSION = 4,
        WHEEL_REFERENCE_SIZE = 5,
        H_PATTERN_SIZE = 17,
        AUXILIARY_AXIS_SIZE = 5,
    };
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings);
    settings.auxiliary_axis = (AuxiliaryAxisSettings){
        .minimum = 300,
        .maximum = 3700,
        .reset_on_start = false,
    };
    settings.steering_limits.percent[0] = 25;
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    uint8_t *record = storage[PLATFORM_STORAGE_SETTINGS_A];
    uint16_t payload_size =
        TUNING_PROFILE_RECORD_SIZE + WHEEL_REFERENCE_SIZE + H_PATTERN_SIZE + AUXILIARY_AXIS_SIZE;
    uint16_t data_size = HEADER_SIZE + payload_size;
    record[4] = AUXILIARY_AXIS_VERSION;
    write_u16(record, 6, payload_size);
    write_u16(record, data_size, checksum(record, data_size));

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored));
    assert(loaded.dirty);
    assert(restored.auxiliary_axis.minimum == 300);
    assert(restored.auxiliary_axis.maximum == 3700);
    assert(!restored.auxiliary_axis.reset_on_start);
    assert(restored.steering_limits.percent[0] == 100);
}

static void test_steering_limit_record_is_upgraded(void) {
    enum { HEADER_SIZE = 12, STEERING_LIMIT_VERSION = 5 };
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings);
    settings.steering_limits.percent[0] = 25;
    settings.wheel_auxiliary_disabled = true;
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    uint8_t *record = storage[PLATFORM_STORAGE_SETTINGS_A];
    uint16_t payload_size = (uint16_t)record[6] | (uint16_t)record[7] << 8;
    payload_size -= 5;
    uint16_t data_size = HEADER_SIZE + payload_size;
    record[4] = STEERING_LIMIT_VERSION;
    write_u16(record, 6, payload_size);
    write_u16(record, data_size, checksum(record, data_size));

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored));
    assert(loaded.dirty);
    assert(restored.steering_limits.percent[0] == 25);
    assert(!restored.wheel_auxiliary_disabled);
}

static void test_auxiliary_output_record_is_upgraded(void) {
    enum { HEADER_SIZE = 12, AUXILIARY_OUTPUT_VERSION = 6, SECURITY_CODE_SIZE = 4 };
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings);
    settings.wheel_auxiliary_disabled = true;
    settings.security_code = (SecurityCodeSettings){
        .digits = {4, 2, 7},
        .enabled = true,
    };
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    uint8_t *record = storage[PLATFORM_STORAGE_SETTINGS_A];
    uint16_t payload_size = (uint16_t)record[6] | (uint16_t)record[7] << 8;
    payload_size -= SECURITY_CODE_SIZE;
    uint16_t data_size = HEADER_SIZE + payload_size;
    record[4] = AUXILIARY_OUTPUT_VERSION;
    write_u16(record, 6, payload_size);
    write_u16(record, data_size, checksum(record, data_size));

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored));
    assert(loaded.dirty);
    assert(restored.wheel_auxiliary_disabled);
    assert(!restored.security_code.enabled);
    assert(restored.security_code.digits[0] == 0);
    assert(restored.security_code.digits[1] == 0);
    assert(restored.security_code.digits[2] == 0);
}

static void test_interrupted_replacement_preserves_previous_record(void) {
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings);
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    settings.tuning_profiles.slots[1].rotation_degrees = 540;
    base_settings_persistence_mark_dirty(&persistence);
    replace_fails = true;
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_RETRY);

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored));
    assert(restored.tuning_profiles.slots[1].rotation_degrees != 540);
}

static void test_corrupted_new_record_falls_back_to_previous_record(void) {
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings);
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);
    uint16_t previous_rotation = settings.tuning_profiles.slots[1].rotation_degrees;

    settings.tuning_profiles.slots[1].rotation_degrees = 360;
    base_settings_persistence_mark_dirty(&persistence);
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);
    storage[PLATFORM_STORAGE_SETTINGS_B][20] ^= 1;

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored));
    assert(restored.tuning_profiles.slots[1].rotation_degrees == previous_rotation);
}

static void test_generation_rollover_selects_new_record(void) {
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings);
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    settings.tuning_profiles.slots[1].rotation_degrees = 360;
    base_settings_persistence_mark_dirty(&persistence);
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);
    set_generation(PLATFORM_STORAGE_SETTINGS_A, UINT32_MAX);
    set_generation(PLATFORM_STORAGE_SETTINGS_B, 0);

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored));
    assert(loaded.active_slot == PLATFORM_STORAGE_SETTINGS_B);
    assert(restored.tuning_profiles.slots[1].rotation_degrees == 360);
}

static void test_clean_settings_are_not_rewritten(void) {
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings);
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_IDLE);
    assert(replace_count == 1);
}

int main(void) {
    test_defaults_are_saved_on_initialization();
    test_dirty_changes_wait_for_explicit_save();
    test_standard_profile_is_regenerated();
    test_retained_selection_becomes_active();
    test_wheel_reference_is_persisted();
    test_shifter_calibration_is_persisted();
    test_auxiliary_axis_settings_are_persisted();
    test_steering_limit_settings_are_persisted();
    test_wheel_auxiliary_option_is_persisted();
    test_security_code_is_persisted();
    test_invalid_wheel_auxiliary_option_is_rejected();
    test_invalid_security_code_is_rejected();
    test_profile_only_record_is_upgraded();
    test_wheel_reference_record_is_upgraded();
    test_h_pattern_record_is_upgraded();
    test_auxiliary_axis_record_is_upgraded();
    test_steering_limit_record_is_upgraded();
    test_auxiliary_output_record_is_upgraded();
    test_interrupted_replacement_preserves_previous_record();
    test_corrupted_new_record_falls_back_to_previous_record();
    test_generation_rollover_selects_new_record();
    test_clean_settings_are_not_rewritten();
    return 0;
}
