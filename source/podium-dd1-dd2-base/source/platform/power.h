#ifndef OPENTEC_BASE_PLATFORM_POWER_H
#define OPENTEC_BASE_PLATFORM_POWER_H

#include <stdbool.h>

void platform_power_init(void);
bool platform_power_button_pressed(void);
void platform_power_latch_set(bool enabled);

#endif
