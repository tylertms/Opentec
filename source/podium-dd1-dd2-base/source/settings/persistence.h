#ifndef OPENTEC_BASE_SETTINGS_PERSISTENCE_H
#define OPENTEC_BASE_SETTINGS_PERSISTENCE_H

#include <stdbool.h>
#include <stdint.h>

#include "settings/state.h"

/** @brief Result of a retained-settings save attempt. */
typedef enum {
    BASE_SETTINGS_PERSISTENCE_IDLE,  /**< No save was needed. */
    BASE_SETTINGS_PERSISTENCE_SAVED, /**< All settings were saved. */
    BASE_SETTINGS_PERSISTENCE_RETRY, /**< A write failed and the save should be retried. */
} BaseSettingsPersistenceResult;

/** @brief Runtime state of base-settings persistence. */
typedef struct {
    bool has_record; /**< True when a valid retained record was loaded or saved. */
    bool dirty;      /**< True when settings need to be saved. */
} BaseSettingsPersistence;

/**
 * @brief Loads retained base settings.
 *
 * Starts from defaults and replaces supported values when the persisted settings format is valid.
 *
 * @param[out] persistence Persistence state receiving record and dirty status.
 * @param[out] settings Base settings receiving defaults or retained values.
 * @return true when the retained settings format is valid and values were loaded; false otherwise.
 */
bool base_settings_persistence_load(BaseSettingsPersistence *persistence, BaseSettings *settings);

/**
 * @brief Marks base settings dirty.
 *
 * Defers writes until the next explicit save call.
 *
 * @param[in,out] persistence Persistence state to mark dirty.
 */
void base_settings_persistence_mark_dirty(BaseSettingsPersistence *persistence);

/**
 * @brief Saves changed base settings.
 *
 * Writes supported retained values and clears the dirty state only after every write succeeds. An
 * automatic auxiliary-calibration snapshot stores its mode but leaves the endpoint records intact
 * until manual mode is selected.
 *
 * @param[in,out] persistence Persistence state to update.
 * @param[in] settings Current base settings to save.
 * @return BASE_SETTINGS_PERSISTENCE_IDLE when clean, SAVED after success, or RETRY after failure.
 */
BaseSettingsPersistenceResult base_settings_persistence_save(BaseSettingsPersistence *persistence,
                                                             const BaseSettings *settings);

#endif
