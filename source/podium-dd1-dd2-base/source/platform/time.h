#ifndef OPENTEC_BASE_PLATFORM_TIME_H
#define OPENTEC_BASE_PLATFORM_TIME_H

#include <stdbool.h>
#include <stdint.h>

void platform_time_init(void);
uint32_t platform_time_ms(void);
bool platform_time_reached(uint32_t now, uint32_t deadline);

#endif
