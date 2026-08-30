#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "settings/journal.h"

enum { PHYSICAL_PAGE_COUNT = SETTINGS_JOURNAL_LOGICAL_PAGE_COUNT * SETTINGS_JOURNAL_SLOT_COUNT };

typedef struct {
    SettingsJournalInstruction pages[PHYSICAL_PAGE_COUNT][SETTINGS_JOURNAL_PAGE_INSTRUCTION_COUNT];
    uint32_t reads;
    uint32_t programs;
    uint32_t erases;
    bool fail_read;
    bool fail_program;
    bool fail_erase;
    uint32_t fail_read_at;
    uint32_t fail_program_at;
    uint32_t fail_erase_at;
    uint32_t corrupt_value_at;
    uint32_t corrupt_tag_at;
} SimulatedFlash;

static bool simulated_read(void *context, uint8_t page, uint16_t instruction,
                           SettingsJournalInstruction *value) {
    SimulatedFlash *flash = context;
    flash->reads++;
    if (flash->fail_read || flash->reads == flash->fail_read_at) {
        return false;
    }
    *value = flash->pages[page][instruction];
    return true;
}

static bool simulated_program(void *context, uint8_t page, uint16_t instruction,
                              SettingsJournalInstruction value) {
    SimulatedFlash *flash = context;
    flash->programs++;
    if (flash->fail_program || flash->programs == flash->fail_program_at) {
        return false;
    }
    SettingsJournalInstruction *current = &flash->pages[page][instruction];
    if ((current->value & value.value) != value.value || (current->tag & value.tag) != value.tag) {
        return false;
    }
    *current = value;
    if (flash->programs == flash->corrupt_value_at) {
        current->value ^= 1;
    }
    if (flash->programs == flash->corrupt_tag_at) {
        current->tag ^= 1;
    }
    return true;
}

static bool simulated_program_row(void *context, uint8_t page, uint16_t row,
                                  const SettingsJournalInstruction *values) {
    uint16_t first = row * SETTINGS_JOURNAL_ROW_INSTRUCTION_COUNT;
    for (uint16_t index = 0; index < SETTINGS_JOURNAL_ROW_INSTRUCTION_COUNT; index++) {
        if (!simulated_program(context, page, first + index, values[index])) {
            return false;
        }
    }
    return true;
}

static bool simulated_erase(void *context, uint8_t page) {
    SimulatedFlash *flash = context;
    flash->erases++;
    if (flash->fail_erase || flash->erases == flash->fail_erase_at) {
        return false;
    }
    memset(flash->pages[page], UINT8_MAX, sizeof(flash->pages[page]));
    return true;
}

static const SettingsJournalOperations operations = {
    .read = simulated_read,
    .program = simulated_program,
    .program_row = simulated_program_row,
    .erase = simulated_erase,
};

static void flash_reset(SimulatedFlash *flash) { memset(flash, UINT8_MAX, sizeof(*flash)); }

static void counters_reset(SimulatedFlash *flash) {
    flash->reads = 0;
    flash->programs = 0;
    flash->erases = 0;
    flash->fail_read = false;
    flash->fail_program = false;
    flash->fail_erase = false;
    flash->fail_read_at = 0;
    flash->fail_program_at = 0;
    flash->fail_erase_at = 0;
    flash->corrupt_value_at = 0;
    flash->corrupt_tag_at = 0;
}

static void active_header_set(SimulatedFlash *flash, uint8_t page, uint16_t generation) {
    flash->pages[page][0] = (SettingsJournalInstruction){.value = generation, .tag = 0xf3};
}

static void record_set(SimulatedFlash *flash, uint8_t page, uint16_t instruction, uint8_t tag,
                       uint16_t value) {
    flash->pages[page][instruction] = (SettingsJournalInstruction){.value = value, .tag = tag};
}

static void full_page_set(SimulatedFlash *flash, uint8_t page) {
    active_header_set(flash, page, 0);
    for (uint16_t instruction = 1; instruction < SETTINGS_JOURNAL_PAGE_INSTRUCTION_COUNT;
         instruction++) {
        record_set(flash, page, instruction, (uint8_t)((instruction - 1) % 255), instruction);
    }
}

static void test_empty_flash_is_initialized(void) {
    SimulatedFlash flash;
    SettingsJournal journal;
    flash_reset(&flash);
    counters_reset(&flash);

    assert(settings_journal_initialize(&journal, &operations, &flash));
    assert(journal.initialized);
    assert(journal.active_slot[0] == 0);
    assert(journal.active_slot[1] == 0);
    assert(flash.erases == 2);
    assert(flash.pages[0][0].tag == 0xf3);
    assert(flash.pages[3][0].tag == 0xf3);
}

static void test_values_are_appended_and_read_from_both_logical_pages(void) {
    SimulatedFlash flash;
    SettingsJournal journal;
    uint16_t value;
    flash_reset(&flash);
    counters_reset(&flash);
    assert(settings_journal_initialize(&journal, &operations, &flash));

    assert(settings_journal_write(&journal, 0, 0x1234));
    assert(settings_journal_write(&journal, 0, 0x5678));
    assert(settings_journal_write(&journal, 254, 0xabcd));
    assert(settings_journal_write(&journal, 255, 0x1357));
    assert(settings_journal_write(&journal, 509, 0x2468));
    assert(settings_journal_read(&journal, 0, &value) && value == 0x5678);
    assert(settings_journal_read(&journal, 254, &value) && value == 0xabcd);
    assert(settings_journal_read(&journal, 255, &value) && value == 0x1357);
    assert(settings_journal_read(&journal, 509, &value) && value == 0x2468);
    assert(!settings_journal_read(&journal, 1, &value));
}

static void test_unchanged_value_does_not_program(void) {
    SimulatedFlash flash;
    SettingsJournal journal;
    flash_reset(&flash);
    counters_reset(&flash);
    assert(settings_journal_initialize(&journal, &operations, &flash));
    assert(settings_journal_write(&journal, 17, 9));
    uint32_t programs = flash.programs;
    assert(settings_journal_write(&journal, 17, 9));
    assert(flash.programs == programs);
}

static void test_full_page_is_compacted(void) {
    SimulatedFlash flash;
    SettingsJournal journal;
    uint16_t value;
    flash_reset(&flash);
    counters_reset(&flash);
    assert(settings_journal_initialize(&journal, &operations, &flash));

    for (uint16_t update = 0; update < SETTINGS_JOURNAL_PAGE_INSTRUCTION_COUNT - 1; update++) {
        assert(settings_journal_write(&journal, 42, update));
    }
    assert(journal.active_slot[0] == 1);
    assert(settings_journal_read(&journal, 42, &value));
    assert(value == SETTINGS_JOURNAL_PAGE_INSTRUCTION_COUNT - 2);
    assert(flash.pages[0][0].tag == UINT8_MAX);
    assert(flash.pages[1][0].tag == 0xf3);
}

static void test_interrupted_rotation_retains_predecessor(void) {
    SimulatedFlash flash;
    SettingsJournal journal;
    uint16_t value;
    flash_reset(&flash);
    counters_reset(&flash);
    active_header_set(&flash, 0, 0);
    active_header_set(&flash, 1, 0);
    active_header_set(&flash, 3, 0);
    record_set(&flash, 0, 1, 5, 11);
    record_set(&flash, 1, 1, 5, 12);

    assert(settings_journal_initialize(&journal, &operations, &flash));
    assert(journal.active_slot[0] == 0);
    assert(settings_journal_read(&journal, 5, &value) && value == 11);
    assert(flash.pages[1][0].tag == UINT8_MAX);

    flash_reset(&flash);
    counters_reset(&flash);
    active_header_set(&flash, 0, 1);
    active_header_set(&flash, 2, 0);
    active_header_set(&flash, 3, 0);
    record_set(&flash, 0, 1, 5, 14);
    record_set(&flash, 2, 1, 5, 13);
    assert(settings_journal_initialize(&journal, &operations, &flash));
    assert(journal.active_slot[0] == 2);
    assert(settings_journal_read(&journal, 5, &value) && value == 13);
    assert(flash.pages[0][0].tag == UINT8_MAX);
}

static void test_header_commit_failure_keeps_source_recoverable(void) {
    SimulatedFlash flash;
    SettingsJournal journal;
    uint16_t value;
    flash_reset(&flash);
    counters_reset(&flash);
    full_page_set(&flash, 0);
    active_header_set(&flash, 3, 0);
    assert(settings_journal_initialize(&journal, &operations, &flash));

    counters_reset(&flash);
    flash.fail_program_at = 257;
    assert(!settings_journal_write(&journal, 0, 0x7777));
    assert(flash.pages[0][0].tag == 0xf3);
    assert(flash.pages[1][0].tag == UINT8_MAX);

    counters_reset(&flash);
    assert(settings_journal_initialize(&journal, &operations, &flash));
    assert(settings_journal_write(&journal, 0, 0x7777));
    assert(settings_journal_read(&journal, 0, &value) && value == 0x7777);
}

static void test_source_erase_failure_is_rolled_back_and_recompacted(void) {
    SimulatedFlash flash;
    SettingsJournal journal;
    uint16_t value;
    flash_reset(&flash);
    counters_reset(&flash);
    full_page_set(&flash, 0);
    active_header_set(&flash, 3, 0);
    assert(settings_journal_initialize(&journal, &operations, &flash));

    counters_reset(&flash);
    flash.fail_erase_at = 2;
    assert(!settings_journal_write(&journal, 0, 0x8888));
    assert(flash.pages[0][0].tag == 0xf3);
    assert(flash.pages[1][0].tag == 0xf3);

    counters_reset(&flash);
    assert(settings_journal_initialize(&journal, &operations, &flash));
    assert(journal.active_slot[0] == 1);
    assert(settings_journal_read(&journal, 0, &value));
    assert(value == 1021);
    assert(flash.pages[0][0].tag == UINT8_MAX);
}

static void test_counter_wrap_clears_epoch_marker(void) {
    SimulatedFlash flash;
    SettingsJournal journal;
    flash_reset(&flash);
    counters_reset(&flash);
    full_page_set(&flash, 2);
    flash.pages[2][0].value = 3;
    active_header_set(&flash, 3, 0);
    assert(settings_journal_initialize(&journal, &operations, &flash));
    assert(journal.active_slot[0] == 2);
    assert(settings_journal_write(&journal, 0, 0x9999));
    assert(journal.active_slot[0] == 0);
    assert(flash.pages[0][0].value == 4);
    assert(flash.pages[0][0].tag == 0xe3);
}

static void test_compaction_io_and_eligibility_failures_are_reported(void) {
    SimulatedFlash flash;
    SettingsJournal journal;
    flash_reset(&flash);
    counters_reset(&flash);
    full_page_set(&flash, 0);
    active_header_set(&flash, 3, 0);
    assert(settings_journal_initialize(&journal, &operations, &flash));

    counters_reset(&flash);
    flash.fail_erase_at = 1;
    assert(!settings_journal_write(&journal, 0, 1));

    counters_reset(&flash);
    flash.fail_program_at = 1;
    assert(!settings_journal_write(&journal, 0, 1));

    flash.pages[1][0].tag = 0xef;
    flash.pages[2][0].tag = 0xef;
    counters_reset(&flash);
    assert(!settings_journal_write(&journal, 0, 1));
}

static void test_program_readback_mismatches_are_rejected(void) {
    SimulatedFlash flash;
    SettingsJournal journal;
    flash_reset(&flash);
    counters_reset(&flash);
    assert(settings_journal_initialize(&journal, &operations, &flash));

    counters_reset(&flash);
    flash.corrupt_value_at = 1;
    assert(!settings_journal_write(&journal, 0, 1));

    flash_reset(&flash);
    counters_reset(&flash);
    assert(settings_journal_initialize(&journal, &operations, &flash));
    counters_reset(&flash);
    flash.corrupt_tag_at = 1;
    assert(!settings_journal_write(&journal, 0, 1));
}

static void test_invalid_state_and_io_failures_are_rejected(void) {
    SimulatedFlash flash;
    SettingsJournal journal;
    uint16_t value;
    flash_reset(&flash);
    counters_reset(&flash);
    active_header_set(&flash, 0, 0);
    active_header_set(&flash, 1, 0);
    active_header_set(&flash, 2, 0);
    active_header_set(&flash, 3, 0);
    assert(!settings_journal_initialize(&journal, &operations, &flash));

    flash_reset(&flash);
    counters_reset(&flash);
    for (uint8_t page = 0; page < SETTINGS_JOURNAL_SLOT_COUNT; page++) {
        flash.pages[page][0].tag = 0xef;
    }
    assert(!settings_journal_initialize(&journal, &operations, &flash));

    flash_reset(&flash);
    counters_reset(&flash);
    flash.fail_read = true;
    assert(!settings_journal_initialize(&journal, &operations, &flash));
    flash.fail_read = false;
    counters_reset(&flash);
    flash.fail_read_at = 2;
    assert(!settings_journal_initialize(&journal, &operations, &flash));
    counters_reset(&flash);
    flash.fail_read = false;
    flash.fail_erase = true;
    assert(!settings_journal_initialize(&journal, &operations, &flash));

    flash_reset(&flash);
    counters_reset(&flash);
    active_header_set(&flash, 0, 0);
    active_header_set(&flash, 1, 0);
    active_header_set(&flash, 3, 0);
    flash.fail_erase = true;
    assert(!settings_journal_initialize(&journal, &operations, &flash));

    flash_reset(&flash);
    counters_reset(&flash);
    assert(settings_journal_initialize(&journal, &operations, &flash));
    flash.fail_program = true;
    assert(!settings_journal_write(&journal, 0, 1));
    flash.fail_program = false;
    flash.fail_read = true;
    assert(!settings_journal_write(&journal, 0, 1));
    assert(!settings_journal_read(&journal, 0, &value));
}

static void test_invalid_arguments_are_rejected(void) {
    SimulatedFlash flash;
    SettingsJournal journal = {0};
    SettingsJournalOperations invalid_operations;
    uint16_t value;
    flash_reset(&flash);
    counters_reset(&flash);
    assert(!settings_journal_initialize(NULL, &operations, &flash));
    assert(!settings_journal_initialize(&journal, NULL, &flash));
    invalid_operations = operations;
    invalid_operations.read = NULL;
    assert(!settings_journal_initialize(&journal, &invalid_operations, &flash));
    invalid_operations = operations;
    invalid_operations.program = NULL;
    assert(!settings_journal_initialize(&journal, &invalid_operations, &flash));
    invalid_operations = operations;
    invalid_operations.program_row = NULL;
    assert(!settings_journal_initialize(&journal, &invalid_operations, &flash));
    invalid_operations = operations;
    invalid_operations.erase = NULL;
    assert(!settings_journal_initialize(&journal, &invalid_operations, &flash));
    assert(!settings_journal_read(&journal, 0, &value));
    assert(!settings_journal_read(NULL, 0, &value));
    assert(!settings_journal_write(NULL, 0, 1));
    assert(settings_journal_initialize(&journal, &operations, &flash));
    assert(!settings_journal_read(&journal, SETTINGS_JOURNAL_VALUE_COUNT, &value));
    assert(!settings_journal_read(&journal, 0, NULL));
    assert(!settings_journal_write(&journal, SETTINGS_JOURNAL_VALUE_COUNT, 1));
}

int main(void) {
    test_empty_flash_is_initialized();
    test_values_are_appended_and_read_from_both_logical_pages();
    test_unchanged_value_does_not_program();
    test_full_page_is_compacted();
    test_interrupted_rotation_retains_predecessor();
    test_header_commit_failure_keeps_source_recoverable();
    test_source_erase_failure_is_rolled_back_and_recompacted();
    test_counter_wrap_clears_epoch_marker();
    test_compaction_io_and_eligibility_failures_are_reported();
    test_program_readback_mismatches_are_rejected();
    test_invalid_state_and_io_failures_are_rejected();
    test_invalid_arguments_are_rejected();
    return 0;
}
