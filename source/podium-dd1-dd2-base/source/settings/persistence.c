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
    PERSISTENCE_VERSION = 2,
    PERSISTENCE_HEADER_SIZE = 12,
    PERSISTENCE_REFERENCE_SIZE = 5,
    PERSISTENCE_PAYLOAD_SIZE = TUNING_PROFILE_RECORD_SIZE + PERSISTENCE_REFERENCE_SIZE,
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

static uint16_t read_u16(const uint8_t *data, uint16_t offset) {
    return (uint16_t)data[offset] | ((uint16_t)data[offset + 1] << 8);
}

static uint32_t read_u32(const uint8_t *data, uint16_t offset) {
    return (uint32_t)data[offset] | ((uint32_t)data[offset + 1] << 8) |
           ((uint32_t)data[offset + 2] << 16) | ((uint32_t)data[offset + 3] << 24);
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

static bool generation_is_newer(uint32_t candidate, uint32_t reference) {
    uint32_t distance = candidate - reference;
    return distance != 0 && distance < UINT32_C(0x80000000);
}

static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return now_ms - deadline_ms < UINT32_C(0x80000000);
}

static bool header_valid(const uint8_t *data, uint16_t payload_size) {
    uint8_t version = data[4];
    bool current = version == PERSISTENCE_VERSION && payload_size == PERSISTENCE_PAYLOAD_SIZE;
    bool profile_only =
        version == PERSISTENCE_PROFILE_ONLY_VERSION && payload_size == TUNING_PROFILE_RECORD_SIZE;
    return data[0] == PERSISTENCE_MAGIC_0 && data[1] == PERSISTENCE_MAGIC_1 &&
           data[2] == PERSISTENCE_MAGIC_2 && data[3] == PERSISTENCE_MAGIC_3 && data[5] == 0 &&
           (current || profile_only);
}

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
    stored->needs_upgrade = data[4] != PERSISTENCE_VERSION;
    if (stored->needs_upgrade) {
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
    stored->valid = true;
    return true;
}

static bool record_read(PlatformStorageSlot slot, StoredSettings *stored) {
    uint8_t data[PERSISTENCE_RECORD_SIZE];
    if (!platform_storage_read(slot, data, PERSISTENCE_RECORD_SIZE)) {
        stored->valid = false;
        return false;
    }
    return record_decode(data, stored);
}

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
    write_u16(data, PERSISTENCE_DATA_SIZE, persistence_checksum(data, PERSISTENCE_DATA_SIZE));
    return true;
}

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

bool base_settings_persistence_load(BaseSettingsPersistence *persistence, BaseSettings *settings,
                                    uint32_t now_ms) {
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
        persistence->generation = records[selected].generation;
        persistence->write_after_ms = now_ms + BASE_SETTINGS_SAVE_DELAY_MS;
        persistence->active_slot = selected;
        persistence->has_record = true;
        persistence->dirty = records[selected].needs_upgrade;
        return true;
    }

    base_settings_defaults(settings);
    persistence->generation = 0;
    persistence->write_after_ms = now_ms + BASE_SETTINGS_SAVE_DELAY_MS;
    persistence->active_slot = PLATFORM_STORAGE_SETTINGS_A;
    persistence->has_record = false;
    persistence->dirty = true;
    return false;
}

void base_settings_persistence_mark_dirty(BaseSettingsPersistence *persistence, uint32_t now_ms) {
    persistence->dirty = true;
    persistence->write_after_ms = now_ms + BASE_SETTINGS_SAVE_DELAY_MS;
}

BaseSettingsPersistenceResult
base_settings_persistence_service(BaseSettingsPersistence *persistence,
                                  const BaseSettings *settings, uint32_t now_ms) {
    if (!persistence->dirty || !deadline_reached(now_ms, persistence->write_after_ms)) {
        return BASE_SETTINGS_PERSISTENCE_IDLE;
    }

    uint8_t target = persistence->has_record ? (uint8_t)(persistence->active_slot ^ 1U)
                                             : PLATFORM_STORAGE_SETTINGS_A;
    uint32_t generation = persistence->generation + 1;
    uint8_t data[PERSISTENCE_RECORD_SIZE];
    if (!record_encode(settings, generation, data) ||
        !platform_storage_replace((PlatformStorageSlot)target, data, PERSISTENCE_RECORD_SIZE) ||
        !record_matches((PlatformStorageSlot)target, data)) {
        persistence->write_after_ms = now_ms + BASE_SETTINGS_SAVE_DELAY_MS;
        return BASE_SETTINGS_PERSISTENCE_RETRY;
    }

    persistence->generation = generation;
    persistence->active_slot = target;
    persistence->has_record = true;
    persistence->dirty = false;
    return BASE_SETTINGS_PERSISTENCE_SAVED;
}
