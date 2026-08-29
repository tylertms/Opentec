#include "platform/system.h"

#include <libpic30.h>
#include <stdbool.h>
#include <stdint.h>
#include <xc.h>

enum {
    BOOTLOADER_REQUEST_ADDRESS = 0x4ffe,
    BOOTLOADER_REQUEST_KEY = 0xaa,
};

static const uint32_t BOOTLOADER_SETTLE_DELAY_CYCLES = 0x9000UL * 0x0c81UL * 2UL;

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

/**
 * @brief Restarts the processor through the bootloader handoff.
 *
 * Waits through the peripheral settling interval, stores the bootloader request key at the
 * handoff location, and executes the processor reset instruction.
 *
 */
void platform_system_enter_bootloader(void) {
    __delay32(BOOTLOADER_SETTLE_DELAY_CYCLES);
    *(volatile uint16_t *)BOOTLOADER_REQUEST_ADDRESS = BOOTLOADER_REQUEST_KEY;
    platform_system_reset();
}
