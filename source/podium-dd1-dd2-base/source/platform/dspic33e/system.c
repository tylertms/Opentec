#include "platform/system.h"

#include <libpic30.h>
#include <stdbool.h>
#include <stdint.h>
#include <xc.h>

/**
 * @brief Bootloader handoff and firmware-protection configuration values.
 */
enum {
    BOOTLOADER_REQUEST_ADDRESS =
        0x4ffe,                    /**< Program address used for the bootloader request key. */
    BOOTLOADER_REQUEST_KEY = 0xaa, /**< Value that requests bootloader entry after reset. */
    FIRMWARE_CONFIGURATION_PAGE =
        0xf8, /**< Table page containing the firmware configuration word. */
    FIRMWARE_CONFIGURATION_OFFSET = 0x0004, /**< Table offset of the firmware configuration word. */
    FIRMWARE_CONFIGURATION_LATCH_PAGE =
        0xfa, /**< Table page used for configuration programming latches. */
    FIRMWARE_CONFIGURATION_UNPROTECTED =
        0x000000cf, /**< Factory-unprotected configuration value. */
    FIRMWARE_CONFIGURATION_PROTECTED =
        0x00fd, /**< Protected configuration value written to the low word. */
    NVM_CONFIGURATION_WORD_PROGRAM =
        0x4000, /**< Nonvolatile-memory configuration-word program operation. */
};

/**
 * @brief Delay cycles that let peripherals settle before a bootloader reset.
 */
static const uint32_t BOOTLOADER_SETTLE_DELAY_CYCLES = 0x9000UL * 0x0c81UL * 2UL;

/**
 * @brief Establishes the processor interrupt and digital-I/O baseline.
 *
 * Selects normal interrupt priority without nesting, enables global interrupts, disables analog
 * input on ports B through E and G, and prepares the six board inputs used before their peripheral
 * owners finish initialization.
 *
 */
void platform_system_init(void) {
    SRbits.IPL = 0;
    INTCON1bits.NSTDIS = 1;
    INTCON2bits.GIE = 1;

    ANSELB = 0;
    ANSELC = 0;
    ANSELD = 0;
    ANSELE = 0;
    ANSELG = 0;

    TRISBbits.TRISB9 = 1;
    TRISBbits.TRISB10 = 1;
    TRISDbits.TRISD0 = 1;
    TRISDbits.TRISD11 = 1;
    TRISBbits.TRISB0 = 1;
    TRISBbits.TRISB15 = 1;
}

/**
 * @brief Controls global processor interrupts.
 *
 * Updates the dsPIC global interrupt-enable bit without changing individual interrupt sources.
 *
 * @param[in] enabled True to permit configured interrupts, or false to block them.
 */
void platform_system_interrupts_set(bool enabled) { INTCON2bits.GIE = enabled; }

/**
 * @brief Enables firmware read protection after initial programming.
 *
 * Leaves every configuration other than the factory-programmable unprotected value unchanged. For
 * that startup value, blocks interrupts, programs the protected configuration, waits for the
 * controller to finish, and re-enables global interrupts.
 *
 */
void platform_system_enable_firmware_protection(void) {
    uint16_t saved_table_page = TBLPAG;
    TBLPAG = FIRMWARE_CONFIGURATION_PAGE;
    uint32_t configuration = __builtin_tblrdl(FIRMWARE_CONFIGURATION_OFFSET);
    configuration |= (uint32_t)__builtin_tblrdh(FIRMWARE_CONFIGURATION_OFFSET) << 16;
    TBLPAG = saved_table_page;
    if (configuration != FIRMWARE_CONFIGURATION_UNPROTECTED) {
        return;
    }

    INTCON2bits.GIE = 0;
    TBLPAG = FIRMWARE_CONFIGURATION_LATCH_PAGE;
    __builtin_tblwtl(0, FIRMWARE_CONFIGURATION_PROTECTED);
    NVMADRU = FIRMWARE_CONFIGURATION_PAGE;
    NVMADR = FIRMWARE_CONFIGURATION_OFFSET;
    NVMCON = NVM_CONFIGURATION_WORD_PROGRAM;
    __builtin_write_NVM();
    while (NVMCONbits.WR != 0) {
    }
    TBLPAG = saved_table_page;
    INTCON2bits.GIE = 1;
}

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

/**
 * @brief Handles a primary-oscillator failure trap.
 *
 * Clears the oscillator-failure source and holds the processor in the trap context.
 *
 */
void __attribute__((interrupt, no_auto_psv)) _OscillatorFail(void) {
    INTCON1bits.OSCFAIL = 0;
    for (;;) {
    }
}

/**
 * @brief Handles an address-error trap.
 *
 * Clears the address-error source and holds the processor in the trap context.
 *
 */
void __attribute__((interrupt, no_auto_psv)) _AddressError(void) {
    INTCON1bits.ADDRERR = 0;
    for (;;) {
    }
}

/**
 * @brief Handles a stack-error trap.
 *
 * Clears the stack-error source and holds the processor in the trap context.
 *
 */
void __attribute__((interrupt, no_auto_psv)) _StackError(void) {
    INTCON1bits.STKERR = 0;
    for (;;) {
    }
}

/**
 * @brief Handles a math-error trap.
 *
 * Clears the math-error source and holds the processor in the trap context.
 *
 */
void __attribute__((interrupt, no_auto_psv)) _MathError(void) {
    INTCON1bits.MATHERR = 0;
    for (;;) {
    }
}

/**
 * @brief Handles a DMA-controller error trap.
 *
 * Clears the DMA-controller error source and resumes the interrupted execution context.
 *
 */
void __attribute__((interrupt, no_auto_psv)) _DMACError(void) { INTCON1bits.DMACERR = 0; }
