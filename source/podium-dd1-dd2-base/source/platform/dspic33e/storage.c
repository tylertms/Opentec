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

static const uint32_t profile_storage_page_a = UINT32_C(0x54000);
static const uint32_t profile_storage_page_b = UINT32_C(0x54800);
static const uint16_t platform_storage_pages[FLASH_STORAGE_INSTRUCTION_COUNT]
    __attribute__((space(prog), address(0x54000), noload, used));

static uint32_t slot_address(PlatformStorageSlot slot) {
    return slot == PLATFORM_STORAGE_PROFILE_A ? profile_storage_page_a : profile_storage_page_b;
}

static bool nvm_execute(void) {
    __builtin_disi(6);
    __builtin_write_NVM();
    while (NVMCONbits.WR != 0) {
    }
    bool successful = NVMCONbits.WRERR == 0;
    NVMCONbits.WREN = 0;
    return successful;
}

static bool flash_page_erase(uint32_t address) {
    NVMADRU = (uint16_t)(address >> 16);
    NVMADR = (uint16_t)address;
    NVMCON = NVM_PAGE_ERASE;
    return nvm_execute();
}

static bool flash_row_program(uint32_t address, const uint8_t *data, uint16_t size) {
    TBLPAG = (uint16_t)(address >> 16);
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
    NVMCON = NVM_ROW_PROGRAM;
    return nvm_execute();
}

bool platform_storage_read(PlatformStorageSlot slot, uint8_t *data, uint16_t size) {
    if (slot >= PLATFORM_STORAGE_SLOT_COUNT || size > PLATFORM_STORAGE_SLOT_SIZE) {
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

bool platform_storage_replace(PlatformStorageSlot slot, const uint8_t *data, uint16_t size) {
    if (slot >= PLATFORM_STORAGE_SLOT_COUNT || size > PLATFORM_STORAGE_SLOT_SIZE) {
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
