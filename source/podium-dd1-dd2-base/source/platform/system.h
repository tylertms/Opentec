#ifndef OPENTEC_BASE_PLATFORM_SYSTEM_H
#define OPENTEC_BASE_PLATFORM_SYSTEM_H

#include <stdbool.h>

void platform_system_interrupts_set(bool enabled);
void platform_system_reset(void);

#endif
