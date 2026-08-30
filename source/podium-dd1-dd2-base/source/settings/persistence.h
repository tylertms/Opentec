#ifndef OPENTEC_BASE_SETTINGS_PERSISTENCE_H
#define OPENTEC_BASE_SETTINGS_PERSISTENCE_H

#include <stdbool.h>
#include <stdint.h>

#include "settings/state.h"

typedef enum {
    BASE_SETTINGS_PERSISTENCE_IDLE,
    BASE_SETTINGS_PERSISTENCE_SAVED,
    BASE_SETTINGS_PERSISTENCE_RETRY,
} BaseSettingsPersistenceResult;

typedef struct {
    bool has_record;
    bool dirty;
} BaseSettingsPersistence;

bool base_settings_persistence_load(BaseSettingsPersistence *persistence, BaseSettings *settings);
void base_settings_persistence_mark_dirty(BaseSettingsPersistence *persistence);
BaseSettingsPersistenceResult base_settings_persistence_save(BaseSettingsPersistence *persistence,
                                                             const BaseSettings *settings);

#endif
