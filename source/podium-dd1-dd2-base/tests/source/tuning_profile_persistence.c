#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform/storage.h"
#include "profile/persistence.h"

static uint8_t storage[PLATFORM_STORAGE_SLOT_COUNT][PLATFORM_STORAGE_SLOT_SIZE];
static uint16_t replace_count;
static bool replace_fails;

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
    TuningProfilePersistence persistence;
    TuningProfileBank bank;
    assert(!tuning_profile_persistence_load(&persistence, &bank, 100));
    assert(persistence.dirty);
    assert(bank.selected_slot == 0);
    assert(tuning_profile_persistence_service(&persistence, &bank, 1099) ==
           TUNING_PROFILE_PERSISTENCE_IDLE);
    assert(tuning_profile_persistence_service(&persistence, &bank, 1100) ==
           TUNING_PROFILE_PERSISTENCE_SAVED);
    assert(replace_count == 1);
    assert(!persistence.dirty);

    TuningProfilePersistence loaded;
    TuningProfileBank restored;
    assert(tuning_profile_persistence_load(&loaded, &restored, 2000));
    assert(restored.slots[0].rotation_degrees == bank.slots[0].rotation_degrees);
}

static void test_dirty_changes_are_coalesced(void) {
    reset_storage();
    TuningProfilePersistence persistence;
    TuningProfileBank bank;
    tuning_profile_persistence_load(&persistence, &bank, 0);
    assert(tuning_profile_persistence_service(&persistence, &bank, 1000) ==
           TUNING_PROFILE_PERSISTENCE_SAVED);

    bank.slots[0].rotation_degrees = 720;
    tuning_profile_persistence_mark_dirty(&persistence, 1200);
    tuning_profile_persistence_mark_dirty(&persistence, 1700);
    assert(tuning_profile_persistence_service(&persistence, &bank, 2199) ==
           TUNING_PROFILE_PERSISTENCE_IDLE);
    assert(tuning_profile_persistence_service(&persistence, &bank, 2700) ==
           TUNING_PROFILE_PERSISTENCE_SAVED);
    assert(replace_count == 2);

    TuningProfilePersistence loaded;
    TuningProfileBank restored;
    assert(tuning_profile_persistence_load(&loaded, &restored, 3000));
    assert(restored.slots[0].rotation_degrees == 720);
}

static void test_interrupted_replacement_preserves_previous_record(void) {
    reset_storage();
    TuningProfilePersistence persistence;
    TuningProfileBank bank;
    tuning_profile_persistence_load(&persistence, &bank, 0);
    assert(tuning_profile_persistence_service(&persistence, &bank, 1000) ==
           TUNING_PROFILE_PERSISTENCE_SAVED);

    bank.slots[0].rotation_degrees = 540;
    tuning_profile_persistence_mark_dirty(&persistence, 1100);
    replace_fails = true;
    assert(tuning_profile_persistence_service(&persistence, &bank, 2100) ==
           TUNING_PROFILE_PERSISTENCE_RETRY);

    TuningProfilePersistence loaded;
    TuningProfileBank restored;
    assert(tuning_profile_persistence_load(&loaded, &restored, 2200));
    assert(restored.slots[0].rotation_degrees != 540);
}

static void test_corrupted_new_record_falls_back_to_previous_record(void) {
    reset_storage();
    TuningProfilePersistence persistence;
    TuningProfileBank bank;
    tuning_profile_persistence_load(&persistence, &bank, 0);
    assert(tuning_profile_persistence_service(&persistence, &bank, 1000) ==
           TUNING_PROFILE_PERSISTENCE_SAVED);
    uint16_t previous_rotation = bank.slots[0].rotation_degrees;

    bank.slots[0].rotation_degrees = 360;
    tuning_profile_persistence_mark_dirty(&persistence, 1100);
    assert(tuning_profile_persistence_service(&persistence, &bank, 2100) ==
           TUNING_PROFILE_PERSISTENCE_SAVED);
    storage[PLATFORM_STORAGE_PROFILE_B][20] ^= 1;

    TuningProfilePersistence loaded;
    TuningProfileBank restored;
    assert(tuning_profile_persistence_load(&loaded, &restored, 2200));
    assert(restored.slots[0].rotation_degrees == previous_rotation);
}

static void test_deadline_wraps_safely(void) {
    reset_storage();
    TuningProfilePersistence persistence;
    TuningProfileBank bank;
    tuning_profile_persistence_load(&persistence, &bank, UINT32_MAX - 500);
    assert(tuning_profile_persistence_service(&persistence, &bank, 498) ==
           TUNING_PROFILE_PERSISTENCE_IDLE);
    assert(tuning_profile_persistence_service(&persistence, &bank, 499) ==
           TUNING_PROFILE_PERSISTENCE_SAVED);
}

int main(void) {
    test_defaults_are_saved_after_delay();
    test_dirty_changes_are_coalesced();
    test_interrupted_replacement_preserves_previous_record();
    test_corrupted_new_record_falls_back_to_previous_record();
    test_deadline_wraps_safely();
    return 0;
}
