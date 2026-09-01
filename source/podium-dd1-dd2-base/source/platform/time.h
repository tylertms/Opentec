#ifndef OPENTEC_BASE_PLATFORM_TIME_H
#define OPENTEC_BASE_PLATFORM_TIME_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initializes the platform millisecond timebase.
 *
 * Configures and starts the hardware timer used to maintain monotonic system time.
 */
void platform_time_init(void);

/**
 * @brief Reads the current platform time.
 *
 * Returns the monotonic counter maintained by the platform timer.
 *
 * @return Elapsed platform time in milliseconds.
 */
uint32_t platform_time_ms(void);

/**
 * @brief Tests whether a monotonic deadline has passed.
 *
 * Compares two counter values using wrap-safe modular arithmetic.
 *
 * @param[in] now Current monotonic time.
 * @param[in] deadline Deadline to test.
 * @return True when now is at or after deadline; otherwise false.
 */
bool platform_time_reached(uint32_t now, uint32_t deadline);

#endif
