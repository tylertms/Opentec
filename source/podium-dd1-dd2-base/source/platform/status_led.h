#ifndef OPENTEC_BASE_PLATFORM_STATUS_LED_H
#define OPENTEC_BASE_PLATFORM_STATUS_LED_H

#include <stdbool.h>

void platform_status_led_init(void);
void platform_status_led_set(bool on);

#endif
