#ifndef OPENTEC_BASE_PLATFORM_POWER_H
#define OPENTEC_BASE_PLATFORM_POWER_H

#include <stdbool.h>

/**
 * @brief Initializes the wheel-base power controls.
 *
 * Configures the power-button input and external power-hold output.
 */
void platform_power_init(void);

/**
 * @brief Reports whether the power button is pressed.
 *
 * Reads the board's active-high power-button input.
 *
 * @return True while the power button is pressed; otherwise false.
 */
bool platform_power_button_pressed(void);

/**
 * @brief Controls the external power hold.
 *
 * Drives the power-hold output according to enabled.
 *
 * @param[in] enabled True to retain power; false to release it.
 */
void platform_power_latch_set(bool enabled);

#endif
