#include <assert.h>
#include <stdint.h>

#include "platform/dspic33e/storage_layout.h"
#include "settings/journal.h"

static void test_settings_flash_address_contract(void) {
    volatile uint8_t first_page = 0;
    volatile uint16_t first_instruction = 0;
    assert(DSPIC33E_SETTINGS_FLASH_ORIGIN == UINT32_C(0x01002000));
    assert(DSPIC33E_SETTINGS_FLASH_PAGE_ADDRESS_SIZE == UINT32_C(0x800));
    assert(dspic33e_settings_flash_instruction_address(first_page, first_instruction) ==
           UINT32_C(0x01002000));
    assert(dspic33e_settings_flash_instruction_address(
               first_page, SETTINGS_JOURNAL_PAGE_INSTRUCTION_COUNT - 1) == UINT32_C(0x010027fe));
    assert(dspic33e_settings_flash_instruction_address(
               SETTINGS_JOURNAL_LOGICAL_PAGE_COUNT * SETTINGS_JOURNAL_SLOT_COUNT - 1,
               SETTINGS_JOURNAL_PAGE_INSTRUCTION_COUNT - 1) == UINT32_C(0x01004ffe));
}

int main(void) {
    test_settings_flash_address_contract();
    return 0;
}
