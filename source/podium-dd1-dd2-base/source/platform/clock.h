#ifndef OPENTEC_BASE_PLATFORM_CLOCK_H
#define OPENTEC_BASE_PLATFORM_CLOCK_H

/**
 * @brief Initializes the processor clock trees.
 *
 * Selects and locks the primary system PLL and auxiliary PLL used by platform peripherals.
 */
void platform_clock_init(void);

#endif
