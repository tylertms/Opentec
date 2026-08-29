#include "platform/storage.h"

#include <stdbool.h>
#include <stdint.h>
#include <xc.h>

enum {
    FLASH_PAGE_ADDRESS_SIZE = 0x800,
    FLASH_STORAGE_PAGE_COUNT = 2,
    FLASH_STORAGE_INSTRUCTION_COUNT = FLASH_PAGE_ADDRESS_SIZE * FLASH_STORAGE_PAGE_COUNT / 2,
    FLASH_INSTRUCTION_DATA_SIZE = 3,
    FLASH_ROW_INSTRUCTION_COUNT = 128,
    FLASH_ROW_ADDRESS_SIZE = FLASH_ROW_INSTRUCTION_COUNT * 2,
    NVM_ROW_PROGRAM = 0x4002,
    NVM_PAGE_ERASE = 0x4003,
};

static const uint32_t settings_storage_page_a = UINT32_C(0x54000);
static const uint32_t settings_storage_page_b = UINT32_C(0x54800);
static const uint16_t platform_storage_pages[FLASH_STORAGE_INSTRUCTION_COUNT]
    __attribute__((space(prog), address(0x54000), noload, used));

/**
 * @brief Resolves a retained-settings slot to its flash page.
 *
 * Selects the first reserved page for slot A and the second reserved page for slot B.
 *
 * @param[in] slot Retained-settings slot.
 * @return Program-flash address of the selected slot.
 */
static uint32_t slot_address(PlatformStorageSlot slot) {
    return slot == PLATFORM_STORAGE_SETTINGS_A ? settings_storage_page_a : settings_storage_page_b;
}

/**
 * @brief Executes the configured nonvolatile flash operation.
 *
 * Raises CPU interrupt priority through the unlock and busy interval, restores the prior priority,
 * disables further writes, and reports the controller result.
 *
 * @return True when the flash controller completed without a write error.
 */
static bool nvm_execute(void) {
    uint16_t previous_priority;
    SET_AND_SAVE_CPU_IPL(previous_priority, 7);
    __builtin_write_NVM();
    while (NVMCONbits.WR != 0) {
    }
    bool successful = NVMCONbits.WRERR == 0;
    NVMCONbits.WREN = 0;
    RESTORE_CPU_IPL(previous_priority);
    return successful;
}

/**
 * @brief Erases one aligned program-flash page.
 *
 * Selects the page through a table-write latch and executes page-erase operation 0x4003 while
 * preserving the caller's table page selection.
 *
 * @param[in] address Aligned program-flash page address.
 * @return True when the page erase completed without a write error.
 */
static bool flash_page_erase(uint32_t address) {
    uint16_t saved_table_page = TBLPAG;
    NVMCON = NVM_PAGE_ERASE;
    TBLPAG = (uint16_t)(address >> 16);
    __builtin_tblwtl((uint16_t)address, UINT16_MAX);
    bool successful = nvm_execute();
    TBLPAG = saved_table_page;
    return successful;
}

/**
 * @brief Programs one complete flash row from a compact byte sequence.
 *
 * Loads 128 three-byte instruction latches, pads unused input bytes with erased values, and
 * executes row-program operation 0x4002.
 *
 * @param[in] address Aligned program-flash row address.
 * @param[in] data Compact instruction bytes to program.
 * @param[in] size Number of input bytes; at most one complete row.
 * @return True when row programming completed without a write error.
 */
static bool flash_row_program(uint32_t address, const uint8_t *data, uint16_t size) {
    TBLPAG = (uint16_t)(address >> 16);
    NVMCON = NVM_ROW_PROGRAM;
    for (uint16_t instruction = 0; instruction < FLASH_ROW_INSTRUCTION_COUNT; instruction++) {
        uint16_t source = instruction * FLASH_INSTRUCTION_DATA_SIZE;
        uint8_t low_byte = source < size ? data[source] : UINT8_MAX;
        uint8_t high_byte = source + 1 < size ? data[source + 1] : UINT8_MAX;
        uint8_t upper_byte = source + 2 < size ? data[source + 2] : UINT8_MAX;
        uint16_t offset = (uint16_t)(address + instruction * 2U);
        __builtin_tblwtl(offset, (uint16_t)low_byte | ((uint16_t)high_byte << 8));
        __builtin_tblwth(offset, upper_byte);
    }
    NVMADRU = (uint16_t)(address >> 16);
    NVMADR = (uint16_t)address;
    return nvm_execute();
}

/**
 * @brief Reads compact bytes from one retained-settings slot.
 *
 * Expands each 24-bit flash instruction into three consecutive bytes and preserves the caller's
 * table page selection.
 *
 * @param[in] slot Retained-settings slot to read.
 * @param[out] data Destination for the requested bytes.
 * @param[in] size Number of bytes to read.
 * @return True when the slot and size are valid; otherwise false.
 */
bool platform_storage_read(PlatformStorageSlot slot, uint8_t *data, uint16_t size) {
    if ((unsigned)slot >= PLATFORM_STORAGE_SLOT_COUNT || size > PLATFORM_STORAGE_SLOT_SIZE) {
        return false;
    }

    uint32_t address = slot_address(slot);
    uint16_t saved_table_page = TBLPAG;
    for (uint16_t index = 0; index < size; index += FLASH_INSTRUCTION_DATA_SIZE) {
        TBLPAG = (uint16_t)(address >> 16);
        uint16_t low = __builtin_tblrdl((uint16_t)address);
        uint16_t high = __builtin_tblrdh((uint16_t)address);
        data[index] = (uint8_t)low;
        if (index + 1 < size) {
            data[index + 1] = (uint8_t)(low >> 8);
        }
        if (index + 2 < size) {
            data[index + 2] = (uint8_t)high;
        }
        address += 2;
    }
    TBLPAG = saved_table_page;
    return true;
}

/**
 * @brief Replaces one retained-settings slot.
 *
 * Erases the selected page and programs its first row from the supplied compact byte sequence.
 * Unused row bytes remain erased.
 *
 * @param[in] slot Retained-settings slot to replace.
 * @param[in] data Source bytes to retain.
 * @param[in] size Number of source bytes.
 * @return True when the aligned erase and row program both succeed; otherwise false.
 */
bool platform_storage_replace(PlatformStorageSlot slot, const uint8_t *data, uint16_t size) {
    if ((unsigned)slot >= PLATFORM_STORAGE_SLOT_COUNT || size > PLATFORM_STORAGE_SLOT_SIZE) {
        return false;
    }

    uint32_t address = slot_address(slot);
    if ((address & (FLASH_PAGE_ADDRESS_SIZE - 1U)) != 0 || !flash_page_erase(address)) {
        return false;
    }

    uint16_t saved_table_page = TBLPAG;
    bool successful =
        (address & (FLASH_ROW_ADDRESS_SIZE - 1U)) == 0 && flash_row_program(address, data, size);
    TBLPAG = saved_table_page;
    return successful;
}
