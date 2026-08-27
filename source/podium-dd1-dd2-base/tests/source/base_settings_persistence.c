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

static void test_defaults_are_saved_after_delay(void) {
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    assert(!base_settings_persistence_load(&persistence, &settings, 100));
    assert(persistence.dirty);
    assert(settings.tuning_profiles.selected_slot == 0);
    assert(!settings.wheel_position.calibrated);
    assert(base_settings_persistence_service(&persistence, &settings, 1099) ==
           BASE_SETTINGS_PERSISTENCE_IDLE);
    assert(base_settings_persistence_service(&persistence, &settings, 1100) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);
    assert(replace_count == 1);
    assert(!persistence.dirty);

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored, 2000));
    assert(restored.tuning_profiles.slots[0].rotation_degrees ==
           settings.tuning_profiles.slots[0].rotation_degrees);
    assert(!restored.wheel_position.calibrated);
}

static void test_dirty_changes_are_coalesced(void) {
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings, 0);
    assert(base_settings_persistence_service(&persistence, &settings, 1000) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    settings.tuning_profiles.slots[0].rotation_degrees = 720;
    base_settings_persistence_mark_dirty(&persistence, 1200);
    base_settings_persistence_mark_dirty(&persistence, 1700);
    assert(base_settings_persistence_service(&persistence, &settings, 2199) ==
           BASE_SETTINGS_PERSISTENCE_IDLE);
    assert(base_settings_persistence_service(&persistence, &settings, 2700) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);
    assert(replace_count == 2);

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored, 3000));
    assert(restored.tuning_profiles.slots[0].rotation_degrees == 720);
}

static void test_wheel_reference_is_persisted(void) {
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings, 0);
    assert(wheel_position_reference_capture(&settings.wheel_position, 25000));
    base_settings_persistence_mark_dirty(&persistence, 100);
    assert(base_settings_persistence_service(&persistence, &settings, 1100) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored, 2000));
    assert(restored.wheel_position.calibrated);
    assert(restored.wheel_position.center == 25000);
}

static void test_profile_only_record_is_upgraded(void) {
    enum { HEADER_SIZE = 12, PROFILE_ONLY_VERSION = 1 };
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings, 0);
    settings.tuning_profiles.slots[0].rotation_degrees = 720;
    assert(base_settings_persistence_service(&persistence, &settings, 1000) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    uint8_t *record = storage[PLATFORM_STORAGE_SETTINGS_A];
    uint16_t data_size = HEADER_SIZE + TUNING_PROFILE_RECORD_SIZE;
    record[4] = PROFILE_ONLY_VERSION;
    write_u16(record, 6, TUNING_PROFILE_RECORD_SIZE);
    write_u16(record, data_size, checksum(record, data_size));

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored, 2000));
    assert(loaded.dirty);
    assert(!restored.wheel_position.calibrated);
    assert(restored.tuning_profiles.slots[0].rotation_degrees == 720);
    assert(base_settings_persistence_service(&loaded, &restored, 2999) ==
           BASE_SETTINGS_PERSISTENCE_IDLE);
    assert(base_settings_persistence_service(&loaded, &restored, 3000) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);
}

static void test_interrupted_replacement_preserves_previous_record(void) {
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings, 0);
    assert(base_settings_persistence_service(&persistence, &settings, 1000) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);

    settings.tuning_profiles.slots[0].rotation_degrees = 540;
    base_settings_persistence_mark_dirty(&persistence, 1100);
    replace_fails = true;
    assert(base_settings_persistence_service(&persistence, &settings, 2100) ==
           BASE_SETTINGS_PERSISTENCE_RETRY);

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored, 2200));
    assert(restored.tuning_profiles.slots[0].rotation_degrees != 540);
}

static void test_corrupted_new_record_falls_back_to_previous_record(void) {
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings, 0);
    assert(base_settings_persistence_service(&persistence, &settings, 1000) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);
    uint16_t previous_rotation = settings.tuning_profiles.slots[0].rotation_degrees;

    settings.tuning_profiles.slots[0].rotation_degrees = 360;
    base_settings_persistence_mark_dirty(&persistence, 1100);
    assert(base_settings_persistence_service(&persistence, &settings, 2100) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);
    storage[PLATFORM_STORAGE_SETTINGS_B][20] ^= 1;

    BaseSettingsPersistence loaded;
    BaseSettings restored;
    assert(base_settings_persistence_load(&loaded, &restored, 2200));
    assert(restored.tuning_profiles.slots[0].rotation_degrees == previous_rotation);
}

static void test_deadline_wraps_safely(void) {
    reset_storage();
    BaseSettingsPersistence persistence;
    BaseSettings settings;
    base_settings_persistence_load(&persistence, &settings, UINT32_MAX - 500);
    assert(base_settings_persistence_service(&persistence, &settings, 498) ==
           BASE_SETTINGS_PERSISTENCE_IDLE);
    assert(base_settings_persistence_service(&persistence, &settings, 499) ==
           BASE_SETTINGS_PERSISTENCE_SAVED);
}

int main(void) {
    test_defaults_are_saved_after_delay();
    test_dirty_changes_are_coalesced();
    test_wheel_reference_is_persisted();
    test_profile_only_record_is_upgraded();
    test_interrupted_replacement_preserves_previous_record();
    test_corrupted_new_record_falls_back_to_previous_record();
    test_deadline_wraps_safely();
    return 0;
}
