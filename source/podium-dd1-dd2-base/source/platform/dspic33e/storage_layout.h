#ifndef OPENTEC_BASE_PLATFORM_DSPIC33E_STORAGE_LAYOUT_H
#define OPENTEC_BASE_PLATFORM_DSPIC33E_STORAGE_LAYOUT_H

#include <stdint.h>

/**
 * @brief Extended program-space layout reserved for the settings journal.
 */
enum {
    DSPIC33E_SETTINGS_FLASH_ORIGIN = 0x01002000,
    DSPIC33E_SETTINGS_FLASH_PAGE_ADDRESS_SIZE = 0x800,
};

/**
 * @brief Resolves a physical settings page and instruction to a program address.
 *
 * @param[in] page Physical settings page.
 * @param[in] instruction Instruction offset within the page.
 * @return Extended program address of the instruction.
 */
static inline uint32_t dspic33e_settings_flash_instruction_address(uint8_t page,
                                                                   uint16_t instruction) {
    return DSPIC33E_SETTINGS_FLASH_ORIGIN +
           (uint32_t)page * DSPIC33E_SETTINGS_FLASH_PAGE_ADDRESS_SIZE + (uint32_t)instruction * 2;
}

#endif
