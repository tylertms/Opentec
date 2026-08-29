#include "platform/system.h"

#include <stdbool.h>
#include <xc.h>

/**
 * @brief Controls global processor interrupts.
 *
 * Updates the dsPIC global interrupt-enable bit without changing individual interrupt sources.
 *
 * @param[in] enabled True to permit configured interrupts, or false to block them.
 */
void platform_system_interrupts_set(bool enabled) { INTCON2bits.GIE = enabled; }

/**
 * @brief Restarts the wheel-base processor.
 *
 * Executes the processor reset instruction and does not return to the current firmware session.
 *
 */
void platform_system_reset(void) {
    __asm__ volatile("reset");
    for (;;) {
    }
}
