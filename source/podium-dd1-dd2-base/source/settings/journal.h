#ifndef OPENTEC_BASE_SETTINGS_JOURNAL_H
#define OPENTEC_BASE_SETTINGS_JOURNAL_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Physical layout limits of the retained-settings journal. */
enum {
    SETTINGS_JOURNAL_LOGICAL_PAGE_COUNT = 2, /**< Number of logical settings pages. */
    SETTINGS_JOURNAL_SLOT_COUNT = 3,         /**< Rotating physical slots per logical page. */
    SETTINGS_JOURNAL_PAGE_INSTRUCTION_COUNT = 1024, /**< Instructions per physical page. */
    SETTINGS_JOURNAL_ROW_INSTRUCTION_COUNT = 128,   /**< Instructions programmed per row. */
    SETTINGS_JOURNAL_VALUE_COUNT = 510, /**< Retained value indexes across all logical pages. */
};

/** @brief One retained-settings journal instruction. */
typedef struct {
    uint16_t value; /**< Retained 16-bit value. */
    uint8_t tag;    /**< Value index or journal header tag. */
} SettingsJournalInstruction;

/** @brief Flash operations required by the settings journal. */
typedef struct {
    bool (*read)(void *context, uint8_t page, uint16_t instruction,
                 SettingsJournalInstruction *value); /**< Reads one physical instruction. */
    bool (*program)(void *context, uint8_t page, uint16_t instruction,
                    SettingsJournalInstruction value); /**< Programs one physical instruction. */
    bool (*program_row)(void *context, uint8_t page, uint16_t row,
                        const SettingsJournalInstruction *values); /**< Programs one row. */
    bool (*erase)(void *context, uint8_t page);                    /**< Erases one physical page. */
} SettingsJournalOperations;

/** @brief Runtime state of the retained-settings journal. */
typedef struct {
    SettingsJournalOperations operations; /**< Caller-supplied flash operations. */
    void *context;                        /**< Caller-owned context passed to flash operations. */
    uint8_t active_slot[SETTINGS_JOURNAL_LOGICAL_PAGE_COUNT]; /**< Active slot per logical page. */
    bool initialized; /**< True after successful journal initialization. */
} SettingsJournal;

/**
 * @brief Initializes the retained-settings journal.
 *
 * Discovers or creates committed rotating slots and repairs an interrupted rotation.
 *
 * @param[out] journal Journal state to initialize.
 * @param[in] operations Flash read, program, row-program, and erase operations.
 * @param[in] context Caller-owned context passed to operations.
 * @return true when both logical pages have a usable active slot; false otherwise.
 */
bool settings_journal_initialize(SettingsJournal *journal,
                                 const SettingsJournalOperations *operations, void *context);

/**
 * @brief Reads one retained setting.
 *
 * Returns the newest value recorded for index without changing the journal.
 *
 * @param[in] journal Initialized journal to read.
 * @param[in] index Global retained value index.
 * @param[out] value Destination for the newest stored value.
 * @return true when index has a stored value; false when it is absent, invalid, or unreadable.
 */
bool settings_journal_read(SettingsJournal *journal, uint16_t index, uint16_t *value);

/**
 * @brief Writes one retained setting.
 *
 * Avoids duplicate records, rotates a full page when needed, and verifies the programmed value.
 *
 * @param[in,out] journal Initialized journal to update.
 * @param[in] index Global retained value index.
 * @param[in] value Value to retain.
 * @return true when value is stored or already current; false when writing or validation fails.
 */
bool settings_journal_write(SettingsJournal *journal, uint16_t index, uint16_t value);

#endif
