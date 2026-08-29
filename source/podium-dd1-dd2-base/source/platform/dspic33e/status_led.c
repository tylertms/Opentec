#include "platform/status_led.h"

#include <stdbool.h>
#include <xc.h>

/**
 * @brief Configures the active-low wheel-base status LED.
 *
 * Selects RG14 as an output and drives it high so the LED starts off.
 */
void platform_status_led_init(void) {
    TRISGbits.TRISG14 = 0;
    LATGbits.LATG14 = 1;
}

/**
 * @brief Controls the wheel-base status LED.
 *
 * Drives the active-low RG14 output low to illuminate the LED or high to turn it off.
 *
 * @param[in] on True to illuminate the LED.
 */
void platform_status_led_set(bool on) { LATGbits.LATG14 = on ? 0 : 1; }
