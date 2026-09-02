#include "platform/storage.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <xc.h>

#include "platform/dspic33e/storage_layout.h"
#include "settings/journal.h"

/**
 * @brief Nonvolatile-memory operation codes.
 */
enum {
    NVM_WORD_PROGRAM = 0x4001, /**< Nonvolatile-memory word-program operation. */
    NVM_ROW_PROGRAM = 0x4002,  /**< Nonvolatile-memory row-program operation. */
    NVM_PAGE_ERASE = 0x4003,   /**< Nonvolatile-memory page-erase operation. */
    WRITE_LATCH_PAGE = 0xfa,   /**< Table page used for nonvolatile-memory write latches. */
};

/**
 * @brief Hardware-backed settings journal instance.
 */
static SettingsJournal settings_journal;

/**
 * @brief Resolves a physical journal page and instruction to a program address.
 *
 * Places six consecutive 0x800-address-unit pages in reserved extended program addresses
 * 0x01002000 through 0x01004fff.
 *
 * @param[in] page Physical settings page from zero through five.
 * @param[in] instruction Instruction offset within the page.
 * @return Extended program address of the instruction.
 */
static uint32_t instruction_address(uint8_t page, uint16_t instruction) {
    return dspic33e_settings_flash_instruction_address(page, instruction);
}

/**
 * @brief Executes the configured nonvolatile-memory operation.
 *
 * Masks interrupts through the unlock and busy interval, restores the previous CPU priority, and
 * disables further writes after completion.
 *
 * @return True when the controller completed without a write error; otherwise false.
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
 * @brief Reads one 24-bit settings-journal instruction.
 *
 * Reads the low word and upper byte through table access while preserving the caller's table page.
 *
 * @param[in] context Unused journal context.
 * @param[in] page Physical settings page from zero through five.
 * @param[in] instruction Instruction offset within the page.
 * @param[out] value Low word and upper-byte tag read from flash.
 * @return True after the addressed instruction is read.
 */
static bool flash_instruction_read(void *context, uint8_t page, uint16_t instruction,
                                   SettingsJournalInstruction *value) {
    (void)context;
    uint32_t address = instruction_address(page, instruction);
    uint16_t saved_table_page = TBLPAG;
    TBLPAG = (uint16_t)(address >> 16);
    value->value = __builtin_tblrdl((uint16_t)address);
    value->tag = (uint8_t)__builtin_tblrdh((uint16_t)address);
    TBLPAG = saved_table_page;
    return true;
}

/**
 * @brief Programs one 24-bit settings-journal instruction.
 *
 * Loads the requested instruction and its unchanged paired neighbor into the word-program latches,
 * executes operation 0x4001, and preserves the caller's table page.
 *
 * @param[in] context Unused journal context.
 * @param[in] page Physical settings page from zero through five.
 * @param[in] instruction Instruction offset within the page.
 * @param[in] value Low word and upper-byte tag to program.
 * @return True when word programming completes without a controller error; otherwise false.
 */
static bool flash_instruction_program(void *context, uint8_t page, uint16_t instruction,
                                      SettingsJournalInstruction value) {
    (void)context;
    uint32_t address = instruction_address(page, instruction);
    uint16_t offset = (uint16_t)address;
    uint16_t neighbor_offset = offset ^ 2U;
    uint16_t saved_table_page = TBLPAG;

    TBLPAG = (uint16_t)(address >> 16);
    SettingsJournalInstruction neighbor = {
        .value = __builtin_tblrdl(neighbor_offset),
        .tag = (uint8_t)__builtin_tblrdh(neighbor_offset),
    };

    NVMCON = NVM_WORD_PROGRAM;
    NVMADRU = (uint16_t)(address >> 16);
    NVMADR = offset;
    TBLPAG = WRITE_LATCH_PAGE;
    uint16_t latch_offset = offset & 3U;
    __builtin_tblwtl(latch_offset, value.value);
    __builtin_tblwth(latch_offset, value.tag);
    __builtin_tblwtl(latch_offset ^ 2U, neighbor.value);
    __builtin_tblwth(latch_offset ^ 2U, neighbor.tag);
    bool successful = nvm_execute();
    TBLPAG = saved_table_page;
    return successful;
}

/**
 * @brief Programs one complete settings-journal instruction row.
 *
 * Loads 128 low words and upper-byte tags into the row latches in instruction order, executes
 * operation 0x4002, and preserves the caller's table page.
 *
 * @param[in] context Unused journal context.
 * @param[in] page Physical settings page from zero through five.
 * @param[in] row Row index within the physical page.
 * @param[in] values Complete row contents in instruction order.
 * @return True when row programming completes without a controller error; otherwise false.
 */
static bool flash_instruction_row_program(void *context, uint8_t page, uint16_t row,
                                          const SettingsJournalInstruction *values) {
    (void)context;
    uint16_t first_instruction = row * SETTINGS_JOURNAL_ROW_INSTRUCTION_COUNT;
    uint32_t address = instruction_address(page, first_instruction);
    uint16_t saved_table_page = TBLPAG;
    NVMCON = NVM_ROW_PROGRAM;
    for (uint16_t index = 0; index < SETTINGS_JOURNAL_ROW_INSTRUCTION_COUNT; index++) {
        uint16_t offset = (uint16_t)(address + (uint32_t)index * 2);
        NVMADRU = (uint16_t)(address >> 16);
        NVMADR = offset;
        TBLPAG = WRITE_LATCH_PAGE;
        __builtin_tblwtl(offset & 0xffU, values[index].value);
        __builtin_tblwth(offset & 0xffU, values[index].tag);
    }
    bool successful = nvm_execute();
    TBLPAG = saved_table_page;
    return successful;
}

/**
 * @brief Erases one physical settings-journal page.
 *
 * Selects the aligned extended program address and executes page-erase operation 0x4003 while
 * preserving the caller's table page.
 *
 * @param[in] context Unused journal context.
 * @param[in] page Physical settings page from zero through five.
 * @return True when the page erase completes without a controller error; otherwise false.
 */
static bool flash_page_erase(void *context, uint8_t page) {
    (void)context;
    uint32_t address = instruction_address(page, 0);
    uint16_t saved_table_page = TBLPAG;
    NVMCON = NVM_PAGE_ERASE;
    NVMADRU = (uint16_t)(address >> 16);
    NVMADR = (uint16_t)address;
    TBLPAG = WRITE_LATCH_PAGE;
    __builtin_tblwtl((uint16_t)address & 0xffU, UINT16_MAX);
    bool successful = nvm_execute();
    TBLPAG = saved_table_page;
    return successful;
}

/**
 * @brief Flash operations supplied to the settings journal.
 */
static const SettingsJournalOperations settings_journal_operations = {
    .read = flash_instruction_read,
    .program = flash_instruction_program,
    .program_row = flash_instruction_row_program,
    .erase = flash_page_erase,
};

/**
 * @brief Initializes retained settings storage.
 *
 * Examines and repairs both three-page journal groups, creating empty active pages when storage is
 * erased.
 *
 * @return True when both settings journal pages are initialized; otherwise false.
 */
bool platform_storage_initialize(void) {
    return settings_journal_initialize(&settings_journal, &settings_journal_operations, NULL);
}

/**
 * @brief Reads one retained 16-bit setting.
 *
 * Returns the newest appended record for the requested settings index.
 *
 * @param[in] index Settings index from zero through 509.
 * @param[out] value Retained value.
 * @return True when the index has a retained value; otherwise false.
 */
bool platform_storage_value_read(uint16_t index, uint16_t *value) {
    return settings_journal_read(&settings_journal, index, value);
}

/**
 * @brief Writes one retained 16-bit setting.
 *
 * Appends the value to the settings journal and performs page rotation when required.
 *
 * @param[in] index Settings index from zero through 509.
 * @param[in] value Value to retain.
 * @return True when the value is stored or already current; otherwise false.
 */
bool platform_storage_value_write(uint16_t index, uint16_t value) {
    return settings_journal_write(&settings_journal, index, value);
}
