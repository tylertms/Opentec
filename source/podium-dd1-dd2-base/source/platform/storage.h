#ifndef OPENTEC_BASE_PLATFORM_STORAGE_H
#define OPENTEC_BASE_PLATFORM_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PLATFORM_STORAGE_PROFILE_A,
    PLATFORM_STORAGE_PROFILE_B,
    PLATFORM_STORAGE_SLOT_COUNT,
} PlatformStorageSlot;

enum { PLATFORM_STORAGE_SLOT_SIZE = 192 };

bool platform_storage_read(PlatformStorageSlot slot, uint8_t *data, uint16_t size);
bool platform_storage_replace(PlatformStorageSlot slot, const uint8_t *data, uint16_t size);

#endif
