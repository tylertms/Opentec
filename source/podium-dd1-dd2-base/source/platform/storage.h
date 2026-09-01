#ifndef OPENTEC_BASE_PLATFORM_STORAGE_H
#define OPENTEC_BASE_PLATFORM_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Number of indexed values provided by platform storage.
 */
enum {
    PLATFORM_STORAGE_VALUE_COUNT =
        510 /**< Number of indexed values provided by platform storage. */
};

/**
 * @brief Initializes retained platform storage.
 *
 * Inspects and repairs the flash journal before values are read or written.
 *
 * @return True when storage is ready; otherwise false.
 */
bool platform_storage_initialize(void);

/**
 * @brief Reads one retained platform-storage value.
 *
 * Retrieves the newest value for an indexed setting.
 *
 * @param[in] index Value index from zero through PLATFORM_STORAGE_VALUE_COUNT - 1.
 * @param[out] value Destination for the retained value.
 * @return True when a value was available; otherwise false.
 */
bool platform_storage_value_read(uint16_t index, uint16_t *value);

/**
 * @brief Writes one retained platform-storage value.
 *
 * Appends the value to the flash journal and performs compaction when required.
 *
 * @param[in] index Value index from zero through PLATFORM_STORAGE_VALUE_COUNT - 1.
 * @param[in] value Value to retain.
 * @return True when the value was stored or was already current; otherwise false.
 */
bool platform_storage_value_write(uint16_t index, uint16_t value);

#endif
