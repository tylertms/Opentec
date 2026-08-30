#ifndef OPENTEC_BASE_SETTINGS_JOURNAL_H
#define OPENTEC_BASE_SETTINGS_JOURNAL_H

#include <stdbool.h>
#include <stdint.h>

enum {
    SETTINGS_JOURNAL_LOGICAL_PAGE_COUNT = 2,
    SETTINGS_JOURNAL_SLOT_COUNT = 3,
    SETTINGS_JOURNAL_PAGE_INSTRUCTION_COUNT = 1024,
    SETTINGS_JOURNAL_ROW_INSTRUCTION_COUNT = 128,
    SETTINGS_JOURNAL_VALUE_COUNT = 510,
};

typedef struct {
    uint16_t value;
    uint8_t tag;
} SettingsJournalInstruction;

typedef struct {
    bool (*read)(void *context, uint8_t page, uint16_t instruction,
                 SettingsJournalInstruction *value);
    bool (*program)(void *context, uint8_t page, uint16_t instruction,
                    SettingsJournalInstruction value);
    bool (*program_row)(void *context, uint8_t page, uint16_t row,
                        const SettingsJournalInstruction *values);
    bool (*erase)(void *context, uint8_t page);
} SettingsJournalOperations;

typedef struct {
    SettingsJournalOperations operations;
    void *context;
    uint8_t active_slot[SETTINGS_JOURNAL_LOGICAL_PAGE_COUNT];
    bool initialized;
} SettingsJournal;

bool settings_journal_initialize(SettingsJournal *journal,
                                 const SettingsJournalOperations *operations, void *context);
bool settings_journal_read(SettingsJournal *journal, uint16_t index, uint16_t *value);
bool settings_journal_write(SettingsJournal *journal, uint16_t index, uint16_t value);

#endif
