#include "platform/power.h"

#include <stdbool.h>
#include <xc.h>

/**
 * @brief Configures the wheel-base profile-save input and hold output.
 *
 * Selects RD9 as the active-high profile-save input and RD8 as the power-hold output without
 * writing the output latch before the first profile-save sample.
 */
void platform_power_init(void) {
    TRISDbits.TRISD9 = 1;
    TRISDbits.TRISD8 = 0;
}

/**
 * @brief Reads the wheel-base profile-save input.
 *
 * Samples active-high RD9.
 *
 * @return True while RD9 is asserted.
 */
bool platform_profile_save_input_active(void) { return PORTDbits.RD9 != 0; }

/**
 * @brief Controls the wheel-base power-hold output.
 *
 * Drives RD8 high to retain base power or low to release the external power hold.
 *
 * @param[in] enabled True to retain power.
 */
void platform_power_latch_set(bool enabled) { LATDbits.LATD8 = enabled; }
