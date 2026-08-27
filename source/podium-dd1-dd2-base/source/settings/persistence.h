#ifndef OPENTEC_BASE_SETTINGS_PERSISTENCE_H
#define OPENTEC_BASE_SETTINGS_PERSISTENCE_H

#include <stdbool.h>
#include <stdint.h>

#include "settings/state.h"

enum { BASE_SETTINGS_SAVE_DELAY_MS = 1000 };

typedef enum {
    BASE_SETTINGS_PERSISTENCE_IDLE,
    BASE_SETTINGS_PERSISTENCE_SAVED,
    BASE_SETTINGS_PERSISTENCE_RETRY,
} BaseSettingsPersistenceResult;

typedef struct {
    uint32_t generation;
    uint32_t write_after_ms;
    uint8_t active_slot;
    bool has_record;
    bool dirty;
} BaseSettingsPersistence;

bool base_settings_persistence_load(BaseSettingsPersistence *persistence, BaseSettings *settings,
                                    uint32_t now_ms);
void base_settings_persistence_mark_dirty(BaseSettingsPersistence *persistence, uint32_t now_ms);
BaseSettingsPersistenceResult
base_settings_persistence_service(BaseSettingsPersistence *persistence,
                                  const BaseSettings *settings, uint32_t now_ms);

#endif
