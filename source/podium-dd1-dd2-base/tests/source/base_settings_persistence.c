#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform/storage.h"
#include "settings/persistence.h"
#include "settings/state.h"

static uint16_t storage[PLATFORM_STORAGE_VALUE_COUNT];
static bool present[PLATFORM_STORAGE_VALUE_COUNT];
static uint16_t write_count;
static bool initialize_fails;
static bool write_fails;
static uint16_t fail_write_index;

bool platform_storage_initialize(void) { return !initialize_fails; }

bool platform_storage_value_read(uint16_t index, uint16_t *value) {
    if (index >= PLATFORM_STORAGE_VALUE_COUNT || !present[index]) {
        return false;
    }
    *value = storage[index];
    return true;
}

bool platform_storage_value_write(uint16_t index, uint16_t value) {
    if (index >= PLATFORM_STORAGE_VALUE_COUNT || write_fails || index == fail_write_index) {
        return false;
    }
    if (!present[index] || storage[index] != value) {
        write_count++;
    }
    storage[index] = value;
    present[index] = true;
    return true;
}

static void storage_reset(void) {
    memset(storage, 0, sizeof(storage));
    memset(present, 0, sizeof(present));
    write_count = 0;
    initialize_fails = false;
    write_fails = false;
    fail_write_index = UINT16_MAX;
}

static void test_erased_storage_loads_defaults_and_saves_reference_format(void) {
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    storage_reset();

    assert(!base_settings_persistence_load(&persistence, &settings));
    assert(!persistence.has_record);
    assert(persistence.dirty);
    assert(settings.tuning_profiles.selected_slot == 0);
    assert(settings.tuning_profiles.standard_mode_enabled);
    assert(!settings.wheel_position.calibrated);
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);
    assert(storage[0] == 0x0300);
    assert(storage[5] == 1);
    assert(storage[6] == 1);
    assert(storage[18] == 0);
    assert(storage[27] == 0x0f38);
    assert(storage[28] == 0x00c8);
    assert(storage[29] == 1);
    assert(storage[26] == 0xaa00);
    for (uint8_t profile = 0; profile < TUNING_PROFILE_SLOT_COUNT; profile++) {
        assert(storage[20 + profile] == 0xaa64);
        assert(present[30 + profile * 26]);
        assert(!present[30 + profile * 26 + 25]);
    }
    assert(persistence.has_record);
    assert(!persistence.dirty);
}

static void test_all_supported_settings_round_trip(void) {
    BaseSettingsPersistence persistence;
    BaseSettings expected;
    storage_reset();
    assert(!base_settings_persistence_load(&persistence, &expected));

    expected.wheel_position = (WheelPositionReference){.center = -1234567, .calibrated = true};
    expected.tuning_profiles.standard_mode_enabled = false;
    expected.tuning_profiles.selected_slot = 4;
    expected.tuning_profiles.active_slot = 4;
    expected.tuning_profiles.slots[2].automatic_rotation = 0;
    expected.tuning_profiles.slots[2].rotation_degrees = 1440;
    expected.tuning_profiles.slots[2].force_feedback_strength = 87;
    expected.tuning_profiles.slots[2].natural_friction = 31;
    expected.tuning_profiles.slots[2].throttle_pedal_curve = TUNING_PEDAL_CURVE_PROGRESSIVE;
    expected.h_pattern_shifter.calibrated = true;
    uint16_t *thresholds = &expected.h_pattern_shifter.calibration.reverse_first_boundary;
    for (uint8_t index = 0; index < 8; index++) {
        thresholds[index] = (uint16_t)(1000 + index * 100);
    }
    expected.security_code.enabled = true;
    expected.security_code.digits[0] = 2;
    expected.security_code.digits[1] = 7;
    expected.security_code.digits[2] = 4;
    for (uint8_t profile = 0; profile < TUNING_PROFILE_SLOT_COUNT; profile++) {
        expected.steering_limits.percent[profile] = (uint8_t)(90 + profile);
    }
    expected.auxiliary_axis.minimum = 250;
    expected.auxiliary_axis.maximum = 3900;
    expected.auxiliary_axis.reset_on_start = false;
    expected.wheel_auxiliary_option = 1;
    storage[55] = 0xbeef;
    present[55] = true;

    assert(base_settings_persistence_save(&persistence, &expected) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);
    assert(storage[18] == 0xa472);
    assert(storage[26] == 0xaa01);
    assert(storage[6] == 5);
    assert(storage[55] == 0xbeef);

    BaseSettingsPersistence loaded_state;
    BaseSettings actual;
    assert(base_settings_persistence_load(&loaded_state, &actual));
    assert(actual.wheel_position.calibrated);
    assert(actual.wheel_position.center == expected.wheel_position.center);
    assert(!actual.tuning_profiles.standard_mode_enabled);
    assert(actual.tuning_profiles.selected_slot == 4);
    assert(actual.tuning_profiles.active_slot == 4);
    assert(!actual.tuning_profiles.slots[2].automatic_rotation);
    assert(actual.tuning_profiles.slots[2].rotation_degrees == 1440);
    assert(actual.tuning_profiles.slots[2].force_feedback_strength == 87);
    assert(actual.tuning_profiles.slots[2].natural_friction == 31);
    assert(actual.tuning_profiles.slots[2].throttle_pedal_curve == TUNING_PEDAL_CURVE_PROGRESSIVE);
    assert(actual.h_pattern_shifter.calibrated);
    for (uint8_t index = 0; index < 8; index++) {
        uint16_t *actual_thresholds = &actual.h_pattern_shifter.calibration.reverse_first_boundary;
        assert(actual_thresholds[index] == thresholds[index]);
    }
    assert(actual.security_code.enabled);
    assert(actual.security_code.digits[0] == 2);
    assert(actual.security_code.digits[1] == 7);
    assert(actual.security_code.digits[2] == 4);
    for (uint8_t profile = 0; profile < TUNING_PROFILE_SLOT_COUNT; profile++) {
        assert(actual.steering_limits.percent[profile] == 90 + profile);
    }
    assert(actual.auxiliary_axis.minimum == 250);
    assert(actual.auxiliary_axis.maximum == 3900);
    assert(!actual.auxiliary_axis.reset_on_start);
    assert(actual.wheel_auxiliary_option == 0);
    assert(storage[26] == 0xaa00);
}

static void test_unformatted_and_invalid_values_keep_defaults(void) {
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    storage_reset();
    storage[0] = 0x0300;
    present[0] = true;
    storage[5] = 3;
    present[5] = true;
    storage[6] = 0;
    present[6] = true;
    storage[18] = 0xaefa;
    present[18] = true;
    storage[20] = 0xaa65;
    present[20] = true;
    storage[27] = 3900;
    present[27] = true;
    storage[28] = 250;
    present[28] = true;

    assert(base_settings_persistence_load(&persistence, &settings));
    assert(settings.tuning_profiles.standard_mode_enabled);
    assert(settings.tuning_profiles.selected_slot == 0);
    assert(!settings.security_code.enabled);
    assert(settings.steering_limits.percent[0] == 100);
    assert(settings.auxiliary_axis.minimum == 0x0f38);
    assert(settings.auxiliary_axis.maximum == 0x00c8);

    storage[0] = 0x1234;
    assert(!base_settings_persistence_load(&persistence, &settings));
    assert(persistence.dirty);
}

static void test_partial_and_invalid_reference_fields_keep_defaults(void) {
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    storage_reset();
    storage[0] = 0x0300;
    present[0] = true;
    storage[1] = 0x1234;
    present[1] = true;
    storage[6] = 7;
    present[6] = true;
    storage[17] = 1;
    present[17] = true;
    storage[18] = 0x9000;
    present[18] = true;
    storage[20] = 0xbb32;
    present[20] = true;
    storage[27] = 250;
    present[27] = true;
    storage[29] = 2;
    present[29] = true;
    storage[26] = 2;
    present[26] = true;

    assert(base_settings_persistence_load(&persistence, &settings));
    assert(!settings.wheel_position.calibrated);
    assert(settings.tuning_profiles.selected_slot == 0);
    assert(!settings.h_pattern_shifter.calibrated);
    assert(!settings.security_code.enabled);
    assert(settings.steering_limits.percent[0] == 100);
    assert(settings.auxiliary_axis.minimum == 0x0f38);
    assert(settings.auxiliary_axis.reset_on_start);
    assert(settings.wheel_auxiliary_option == 2);
}

static void test_save_validation_and_each_write_boundary(void) {
    static const uint16_t failure_indices[] = {1, 2, 5, 6, 7, 17, 18, 20, 26, 27, 28, 29, 30, 0};
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    for (uint8_t failure = 0; failure < sizeof(failure_indices) / sizeof(failure_indices[0]);
         failure++) {
        storage_reset();
        base_settings_defaults(&settings);
        persistence = (BaseSettingsPersistence){.dirty = true};
        fail_write_index = failure_indices[failure];
        assert(base_settings_persistence_save(&persistence, &settings) ==
               BASE_SETTINGS_PERSISTENCE_RETRY);
    }

    storage_reset();
    base_settings_defaults(&settings);
    persistence = (BaseSettingsPersistence){.dirty = true};
    settings.tuning_profiles.selected_slot = TUNING_PROFILE_SLOT_COUNT;
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_RETRY);

    storage_reset();
    base_settings_defaults(&settings);
    persistence = (BaseSettingsPersistence){.dirty = true};
    settings.security_code.enabled = true;
    settings.security_code.digits[1] = 10;
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_RETRY);

    storage_reset();
    base_settings_defaults(&settings);
    persistence = (BaseSettingsPersistence){.dirty = true};
    settings.steering_limits.percent[2] = 101;
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_RETRY);
}

static void test_dirty_and_failure_behavior(void) {
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    storage_reset();
    initialize_fails = true;
    assert(!base_settings_persistence_load(&persistence, &settings));
    initialize_fails = false;
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_IDLE);

    base_settings_persistence_mark_dirty(&persistence);
    write_fails = true;
    assert(base_settings_persistence_save(&persistence, &settings) ==
           BASE_SETTINGS_PERSISTENCE_RETRY);
    assert(persistence.dirty);
}

int main(void) {
    test_erased_storage_loads_defaults_and_saves_reference_format();
    test_all_supported_settings_round_trip();
    test_unformatted_and_invalid_values_keep_defaults();
    test_partial_and_invalid_reference_fields_keep_defaults();
    test_save_validation_and_each_write_boundary();
    test_dirty_and_failure_behavior();
    return 0;
}
