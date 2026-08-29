#include "platform/time.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Tests whether a monotonic deadline has passed.
 *
 * Uses signed modular subtraction so comparisons remain correct across a 32-bit counter wrap.
 *
 * @param[in] now Current monotonic time.
 * @param[in] deadline Monotonic deadline to test.
 * @return True when the deadline is current or past; otherwise false.
 */
bool platform_time_reached(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
}
