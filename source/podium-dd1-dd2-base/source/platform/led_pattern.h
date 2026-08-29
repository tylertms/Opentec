#ifndef OPENTEC_BASE_PLATFORM_LED_PATTERN_H
#define OPENTEC_BASE_PLATFORM_LED_PATTERN_H

#include <stdint.h>

void platform_led_pattern_init(void);
void platform_led_pattern_set_duty(uint16_t duty);

#endif
