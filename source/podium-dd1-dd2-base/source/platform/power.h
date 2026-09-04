#ifndef OPENTEC_BASE_PLATFORM_POWER_H
#define OPENTEC_BASE_PLATFORM_POWER_H

#include <stdbool.h>

/**
 * @brief Initializes the wheel-base power controls.
 *
 * Configures the RD9 profile-save input and RD8 external power-hold output.
 */
void platform_power_init(void);

/**
 * @brief Reports whether the physical profile-save input is active.
 *
 * Reads the board's active-high RD9 input.
 *
 * @return True while RD9 is asserted; otherwise false.
 */
bool platform_profile_save_input_active(void);

/**
 * @brief Controls the external power hold.
 *
 * Drives the power-hold output according to enabled.
 *
 * @param[in] enabled True to retain power; false to release it.
 */
void platform_power_latch_set(bool enabled);

#endif
