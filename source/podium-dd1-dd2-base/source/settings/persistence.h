#ifndef OPENTEC_BASE_SETTINGS_PERSISTENCE_H
#define OPENTEC_BASE_SETTINGS_PERSISTENCE_H

#include <stdbool.h>
#include <stdint.h>

#include "settings/state.h"

/** @brief Result of a retained-settings persistence operation. */
typedef enum {
    BASE_SETTINGS_PERSISTENCE_IDLE,  /**< No retained write was needed. */
    BASE_SETTINGS_PERSISTENCE_SAVED, /**< The requested retained values were saved. */
    BASE_SETTINGS_PERSISTENCE_RETRY, /**< The operation failed and should be retried. */
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

/**
 * @brief Persists one bounded steering-limit percentage.
 *
 * Reads the selected profile record before applying the request. A record without the AA marker
 * forces the effective percentage to the default and repairs the record with AA64; a marked
 * record accepts the requested zero-through-100 percentage. Only the selected record is written,
 * and the general dirty state is unchanged.
 *
 * @param[in,out] settings Base settings receiving the effective percentage.
 * @param[in] active_profile Zero-based profile index.
 * @param[in] requested_percent Requested zero-through-100 percentage.
 * @return BASE_SETTINGS_PERSISTENCE_IDLE when no write is needed, SAVED after a successful write,
 * or RETRY after invalid input or a failed write.
 */
BaseSettingsPersistenceResult
base_settings_persistence_set_steering_limit(BaseSettings *settings, uint8_t active_profile,
                                             uint8_t requested_percent);

#endif
