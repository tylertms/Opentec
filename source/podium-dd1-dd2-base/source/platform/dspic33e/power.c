#include "platform/power.h"

#include <stdbool.h>
#include <xc.h>

/**
 * @brief Configures the wheel-base power button and hold output.
 *
 * Selects RD9 as the active-high button input and RD8 as the power-hold output without writing the
 * output latch before the first power-button sample.
 */
void platform_power_init(void) {
    TRISDbits.TRISD9 = 1;
    TRISDbits.TRISD8 = 0;
}

/**
 * @brief Reads the wheel-base power button.
 *
 * Samples the active-high input on RD9.
 *
 * @return True while the power button is pressed.
 */
bool platform_power_button_pressed(void) { return PORTDbits.RD9 != 0; }

/**
 * @brief Controls the wheel-base power-hold output.
 *
 * Drives RD8 high to retain base power or low to release the external power hold.
 *
 * @param[in] enabled True to retain power.
 */
void platform_power_latch_set(bool enabled) { LATDbits.LATD8 = enabled; }
