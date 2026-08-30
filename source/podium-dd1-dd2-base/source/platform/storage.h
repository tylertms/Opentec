#ifndef OPENTEC_BASE_PLATFORM_STORAGE_H
#define OPENTEC_BASE_PLATFORM_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

enum { PLATFORM_STORAGE_VALUE_COUNT = 510 };

bool platform_storage_initialize(void);
bool platform_storage_value_read(uint16_t index, uint16_t *value);
bool platform_storage_value_write(uint16_t index, uint16_t value);

#endif
