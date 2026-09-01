#ifndef OPENTEC_BASE_PLATFORM_LED_PATTERN_H
#define OPENTEC_BASE_PLATFORM_LED_PATTERN_H

#include <stdint.h>

/**
 * @brief Initializes the board LED PWM output.
 *
 * Configures the LED output and starts the timer that applies pending duty updates.
 */
void platform_led_pattern_init(void);

/**
 * @brief Queues a board LED PWM duty.
 *
 * Retains the duty value for application at the next timer boundary.
 *
 * @param[in] duty PWM compare value to apply.
 */
void platform_led_pattern_set_duty(uint16_t duty);

#endif
