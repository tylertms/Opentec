#ifndef OPENTEC_BASE_PLATFORM_SYSTEM_H
#define OPENTEC_BASE_PLATFORM_SYSTEM_H

#include <stdbool.h>

/**
 * @brief Initializes the processor system baseline.
 *
 * Configures global interrupt behavior and the digital-input defaults required by platform
 * peripherals.
 */
void platform_system_init(void);

/**
 * @brief Enables or disables global processor interrupts.
 *
 * Changes the global interrupt-enable state without changing individual interrupt sources.
 *
 * @param[in] enabled True to permit configured interrupts; false to block them.
 */
void platform_system_interrupts_set(bool enabled);

/**
 * @brief Enables firmware read protection when required.
 *
 * Programs the protected configuration only when the device still has its factory-unprotected
 * configuration value.
 */
void platform_system_enable_firmware_protection(void);

/**
 * @brief Resets the processor.
 *
 * Executes the processor reset instruction and does not return to the current firmware session.
 */
void platform_system_reset(void);

/**
 * @brief Requests bootloader entry and resets the processor.
 *
 * Stores the bootloader handoff key before executing the processor reset instruction.
 */
void platform_system_enter_bootloader(void);

#endif
