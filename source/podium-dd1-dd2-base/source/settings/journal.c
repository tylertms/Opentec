#include "settings/journal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/** @brief Internal settings-journal layout and marker constants. */
enum {
    SETTINGS_PER_LOGICAL_PAGE = 255, /**< Value indexes per logical page. */
    HEADER_ACTIVE_MASK = 1 << 3,     /**< Header bit cleared for an active slot. */
    HEADER_EPOCH_MASK = 1 << 4,      /**< Header bit indicating an eligible epoch. */
    HEADER_INITIAL_TAG = 0xf3,       /**< Initial committed header tag. */
    ERASED_TAG = 0xff,               /**< Tag value of an erased instruction. */
};

/** @brief Result of an internal journal search. */
typedef enum {
    JOURNAL_SEARCH_ERROR,  /**< Search failed because storage could not be read. */
    JOURNAL_SEARCH_ABSENT, /**< No matching or free instruction was found. */
    JOURNAL_SEARCH_FOUND,  /**< A matching or free instruction was found. */
} JournalSearchResult;

/**
 * @brief Reads one instruction from a physical settings page.
 *
 * Routes the journal operation through the supplied storage interface.
 *
 * @param[in] journal Settings journal instance.
 * @param[in] page Physical page index from zero through five.
 * @param[in] instruction Instruction offset within the flash page.
 * @param[out] value Low word and upper-byte tag read from the instruction.
 * @return True when the instruction was read; otherwise false.
 */
static bool instruction_read(SettingsJournal *journal, uint8_t page, uint16_t instruction,
                             SettingsJournalInstruction *value) {
    return journal->operations.read(journal->context, page, instruction, value);
}

/**
 * @brief Programs one instruction in a physical settings page.
 *
 * Routes the journal operation through the supplied storage interface and confirms the complete
 * 24-bit instruction after programming.
 *
 * @param[in] journal Settings journal instance.
 * @param[in] page Physical page index from zero through five.
 * @param[in] instruction Instruction offset within the flash page.
 * @param[in] value Low word and upper-byte tag to program.
 * @return True when the programmed instruction matches the requested value; otherwise false.
 */
static bool instruction_program(SettingsJournal *journal, uint8_t page, uint16_t instruction,
                                SettingsJournalInstruction value) {
    SettingsJournalInstruction actual;
    return journal->operations.program(journal->context, page, instruction, value) &&
           instruction_read(journal, page, instruction, &actual) && actual.value == value.value &&
           actual.tag == value.tag;
}

/**
 * @brief Programs and confirms one complete instruction row.
 *
 * Routes a 128-instruction row through the supplied storage interface, then compares every
 * destination instruction with its requested low word and upper-byte tag.
 *
 * @param[in] journal Settings journal instance.
 * @param[in] page Physical page index from zero through five.
 * @param[in] row Row index within the flash page.
 * @param[in] values Complete row contents in instruction order.
 * @return True when every programmed instruction matches; otherwise false.
 */
static bool instruction_row_program(SettingsJournal *journal, uint8_t page, uint16_t row,
                                    const SettingsJournalInstruction *values) {
    if (!journal->operations.program_row(journal->context, page, row, values)) {
        return false;
    }
    uint16_t first = row * SETTINGS_JOURNAL_ROW_INSTRUCTION_COUNT;
    for (uint16_t index = 0; index < SETTINGS_JOURNAL_ROW_INSTRUCTION_COUNT; index++) {
        SettingsJournalInstruction actual;
        if (!instruction_read(journal, page, first + index, &actual) ||
            actual.value != values[index].value || actual.tag != values[index].tag) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Erases one physical settings page.
 *
 * Routes the erase through the supplied storage interface.
 *
 * @param[in] journal Settings journal instance.
 * @param[in] page Physical page index from zero through five.
 * @return True when the page erase completed; otherwise false.
 */
static bool page_erase(SettingsJournal *journal, uint8_t page) {
    return journal->operations.erase(journal->context, page);
}

/**
 * @brief Resolves a logical page and slot to a physical page.
 *
 * Places each logical page in a consecutive group of three rotating flash pages.
 *
 * @param[in] logical_page Logical settings page.
 * @param[in] slot Rotating slot within the logical page.
 * @return Physical page index.
 */
static uint8_t physical_page(uint8_t logical_page, uint8_t slot) {
    return (uint8_t)(logical_page * SETTINGS_JOURNAL_SLOT_COUNT + slot);
}

/**
 * @brief Tests whether a slot header marks the page active.
 *
 * Treats a cleared header bit three as the committed-page marker.
 *
 * @param[in] header Slot header instruction.
 * @return True when the slot is active; otherwise false.
 */
static bool header_is_active(SettingsJournalInstruction header) {
    return (header.tag & HEADER_ACTIVE_MASK) == 0;
}

/**
 * @brief Reads one rotating-slot header.
 *
 * Resolves the logical page and slot, then reads instruction zero.
 *
 * @param[in] journal Settings journal instance.
 * @param[in] logical_page Logical settings page.
 * @param[in] slot Rotating slot within the logical page.
 * @param[out] header Slot header instruction.
 * @return True when the header was read; otherwise false.
 */
static bool header_read(SettingsJournal *journal, uint8_t logical_page, uint8_t slot,
                        SettingsJournalInstruction *header) {
    return instruction_read(journal, physical_page(logical_page, slot), 0, header);
}

/**
 * @brief Finds committed slots for one logical settings page.
 *
 * Scans all three headers in slot order and records each page whose active marker is cleared.
 *
 * @param[in] journal Settings journal instance.
 * @param[in] logical_page Logical settings page.
 * @param[out] active Slots containing committed journals.
 * @return Number of active slots, or 255 when a header cannot be read.
 */
static uint8_t active_slots(SettingsJournal *journal, uint8_t logical_page,
                            uint8_t active[SETTINGS_JOURNAL_SLOT_COUNT]) {
    uint8_t count = 0;
    for (uint8_t slot = 0; slot < SETTINGS_JOURNAL_SLOT_COUNT; slot++) {
        SettingsJournalInstruction header;
        if (!header_read(journal, logical_page, slot, &header)) {
            return UINT8_MAX;
        }
        if (header_is_active(header)) {
            active[count++] = slot;
        }
    }
    return count;
}

/**
 * @brief Confirms that a logical page has a bit-four-eligible slot.
 *
 * Scans all three headers and requires at least one slot whose epoch marker remains set.
 *
 * @param[in] journal Settings journal instance.
 * @param[in] logical_page Logical settings page.
 * @return True when rotation has an eligible slot; otherwise false.
 */
static bool logical_page_has_eligible_slot(SettingsJournal *journal, uint8_t logical_page) {
    for (uint8_t slot = 0; slot < SETTINGS_JOURNAL_SLOT_COUNT; slot++) {
        SettingsJournalInstruction header;
        if (!header_read(journal, logical_page, slot, &header)) {
            return false;
        }
        if ((header.tag & HEADER_EPOCH_MASK) != 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Creates the first committed slot for a logical settings page.
 *
 * Erases slot zero and programs the F3 active header used by the reference settings journal.
 *
 * @param[in] journal Settings journal instance.
 * @param[in] logical_page Logical settings page.
 * @return True when the initial header was committed; otherwise false.
 */
static bool logical_page_create(SettingsJournal *journal, uint8_t logical_page) {
    uint8_t page = physical_page(logical_page, 0);
    SettingsJournalInstruction header = {.value = 0, .tag = HEADER_INITIAL_TAG};
    return page_erase(journal, page) && instruction_program(journal, page, 0, header);
}

/**
 * @brief Recovers an interrupted slot rotation.
 *
 * When two adjacent slots are committed, retains the predecessor and erases its successor. This
 * restores the last complete page that preceded the interrupted retirement step.
 *
 * @param[in] journal Settings journal instance.
 * @param[in] logical_page Logical settings page.
 * @param[in] first First active slot in ascending order.
 * @param[in] second Second active slot in ascending order.
 * @param[out] retained Slot retained as active.
 * @return True when the interrupted rotation was repaired; otherwise false.
 */
static bool logical_page_recover(SettingsJournal *journal, uint8_t logical_page, uint8_t first,
                                 uint8_t second, uint8_t *retained) {
    uint8_t predecessor = first;
    uint8_t successor = second;
    if (first == 0 && second == SETTINGS_JOURNAL_SLOT_COUNT - 1) {
        predecessor = second;
        successor = first;
    }
    if (!page_erase(journal, physical_page(logical_page, successor))) {
        return false;
    }
    *retained = predecessor;
    return true;
}

/**
 * @brief Locates the newest value for a local settings index.
 *
 * Scans the active slot backward so the most recently appended matching tag wins.
 *
 * @param[in] journal Settings journal instance.
 * @param[in] logical_page Logical settings page.
 * @param[in] local_index Index within the logical page.
 * @param[out] value Stored value when a matching record exists.
 * @return JOURNAL_SEARCH_FOUND when a stored value was found, JOURNAL_SEARCH_ABSENT when no value
 * matches, or JOURNAL_SEARCH_ERROR when storage cannot be read.
 */
static JournalSearchResult local_value_find(SettingsJournal *journal, uint8_t logical_page,
                                            uint8_t local_index, uint16_t *value) {
    uint8_t page = physical_page(logical_page, journal->active_slot[logical_page]);
    for (uint16_t instruction = SETTINGS_JOURNAL_PAGE_INSTRUCTION_COUNT - 1; instruction != 0;
         instruction--) {
        SettingsJournalInstruction record;
        if (!instruction_read(journal, page, instruction, &record)) {
            return JOURNAL_SEARCH_ERROR;
        }
        if (record.tag == local_index) {
            *value = record.value;
            return JOURNAL_SEARCH_FOUND;
        }
    }
    return JOURNAL_SEARCH_ABSENT;
}

/**
 * @brief Finds the first unused record in the active slot.
 *
 * Scans forward from the instruction after the header and recognizes an FF upper byte as erased.
 *
 * @param[in] journal Settings journal instance.
 * @param[in] logical_page Logical settings page.
 * @param[out] instruction First unused instruction offset.
 * @return JOURNAL_SEARCH_FOUND when free space exists, JOURNAL_SEARCH_ABSENT when the slot is full,
 * or JOURNAL_SEARCH_ERROR when storage cannot be read.
 */
static JournalSearchResult free_instruction_find(SettingsJournal *journal, uint8_t logical_page,
                                                 uint16_t *instruction) {
    uint8_t page = physical_page(logical_page, journal->active_slot[logical_page]);
    for (uint16_t candidate = 1; candidate < SETTINGS_JOURNAL_PAGE_INSTRUCTION_COUNT; candidate++) {
        SettingsJournalInstruction record;
        if (!instruction_read(journal, page, candidate, &record)) {
            return JOURNAL_SEARCH_ERROR;
        }
        if (record.tag == ERASED_TAG) {
            *instruction = candidate;
            return JOURNAL_SEARCH_FOUND;
        }
    }
    return JOURNAL_SEARCH_ABSENT;
}

/**
 * @brief Rotates a full logical settings page.
 *
 * Copies the newest value of each local index to the next usable slot, commits its header after
 * all records are confirmed, and erases the previous active slot last.
 *
 * @param[in,out] journal Settings journal instance.
 * @param[in] logical_page Logical settings page to compact.
 * @return True when the new slot was committed and the previous slot retired; otherwise false.
 */
static bool logical_page_compact(SettingsJournal *journal, uint8_t logical_page) {
    uint8_t current_slot = journal->active_slot[logical_page];
    uint8_t target_slot = (uint8_t)((current_slot + 1) % SETTINGS_JOURNAL_SLOT_COUNT);
    SettingsJournalInstruction current_header;
    if (!header_read(journal, logical_page, current_slot, &current_header)) {
        return false;
    }

    for (uint8_t attempts = 0; attempts < SETTINGS_JOURNAL_SLOT_COUNT; attempts++) {
        SettingsJournalInstruction candidate_header;
        if (!header_read(journal, logical_page, target_slot, &candidate_header)) {
            return false;
        }
        if ((candidate_header.tag & HEADER_EPOCH_MASK) != 0) {
            break;
        }
        target_slot = (uint8_t)((target_slot + 1) % SETTINGS_JOURNAL_SLOT_COUNT);
    }
    if (target_slot == current_slot) {
        return false;
    }

    uint8_t target_page = physical_page(logical_page, target_slot);
    if (!page_erase(journal, target_page)) {
        return false;
    }

    SettingsJournalInstruction row[SETTINGS_JOURNAL_ROW_INSTRUCTION_COUNT];
    memset(row, UINT8_MAX, sizeof(row));
    uint16_t output = 1;
    for (uint16_t local_index = 0; local_index < SETTINGS_PER_LOGICAL_PAGE; local_index++) {
        uint16_t value;
        JournalSearchResult result =
            local_value_find(journal, logical_page, (uint8_t)local_index, &value);
        if (result == JOURNAL_SEARCH_ERROR) {
            return false;
        }
        if (result == JOURNAL_SEARCH_FOUND) {
            SettingsJournalInstruction record = {.value = value, .tag = (uint8_t)local_index};
            row[output % SETTINGS_JOURNAL_ROW_INSTRUCTION_COUNT] = record;
            output++;
            if (output % SETTINGS_JOURNAL_ROW_INSTRUCTION_COUNT == 0 &&
                !instruction_row_program(journal, target_page,
                                         (output - SETTINGS_JOURNAL_ROW_INSTRUCTION_COUNT) /
                                             SETTINGS_JOURNAL_ROW_INSTRUCTION_COUNT,
                                         row)) {
                return false;
            }
            if (output % SETTINGS_JOURNAL_ROW_INSTRUCTION_COUNT == 0) {
                memset(row, UINT8_MAX, sizeof(row));
            }
        }
    }
    if (output % SETTINGS_JOURNAL_ROW_INSTRUCTION_COUNT != 0 &&
        !instruction_row_program(journal, target_page,
                                 output / SETTINGS_JOURNAL_ROW_INSTRUCTION_COUNT, row)) {
        return false;
    }

    SettingsJournalInstruction target_header = current_header;
    if (target_slot == 0) {
        target_header.value++;
    }
    if (target_header.value > SETTINGS_JOURNAL_SLOT_COUNT) {
        target_header.tag &= (uint8_t)~HEADER_EPOCH_MASK;
    }
    if (!instruction_program(journal, target_page, 0, target_header)) {
        return false;
    }
    if (!page_erase(journal, physical_page(logical_page, current_slot))) {
        return false;
    }
    journal->active_slot[logical_page] = target_slot;
    return true;
}

bool settings_journal_initialize(SettingsJournal *journal,
                                 const SettingsJournalOperations *operations, void *context) {
    if (journal == NULL || operations == NULL || operations->read == NULL ||
        operations->program == NULL || operations->program_row == NULL ||
        operations->erase == NULL) {
        return false;
    }

    journal->operations = *operations;
    journal->context = context;
    journal->initialized = false;
    for (uint8_t logical_page = 0; logical_page < SETTINGS_JOURNAL_LOGICAL_PAGE_COUNT;
         logical_page++) {
        if (!logical_page_has_eligible_slot(journal, logical_page)) {
            return false;
        }
        uint8_t active[SETTINGS_JOURNAL_SLOT_COUNT];
        uint8_t count = active_slots(journal, logical_page, active);
        if (count == 0) {
            if (!logical_page_create(journal, logical_page)) {
                return false;
            }
            journal->active_slot[logical_page] = 0;
        } else if (count == 1) {
            journal->active_slot[logical_page] = active[0];
        } else if (count == 2) {
            if (!logical_page_recover(journal, logical_page, active[0], active[1],
                                      &journal->active_slot[logical_page])) {
                return false;
            }
            uint16_t instruction;
            JournalSearchResult free_result =
                free_instruction_find(journal, logical_page, &instruction);
            if (free_result == JOURNAL_SEARCH_ERROR ||
                (free_result == JOURNAL_SEARCH_ABSENT &&
                 !logical_page_compact(journal, logical_page))) {
                return false;
            }
        } else {
            return false;
        }
    }
    journal->initialized = true;
    return true;
}

bool settings_journal_read(SettingsJournal *journal, uint16_t index, uint16_t *value) {
    if (journal == NULL || value == NULL || !journal->initialized ||
        index >= SETTINGS_JOURNAL_VALUE_COUNT) {
        return false;
    }
    uint8_t logical_page = (uint8_t)(index / SETTINGS_PER_LOGICAL_PAGE);
    uint8_t local_index = (uint8_t)(index % SETTINGS_PER_LOGICAL_PAGE);
    return local_value_find(journal, logical_page, local_index, value) == JOURNAL_SEARCH_FOUND;
}

bool settings_journal_write(SettingsJournal *journal, uint16_t index, uint16_t value) {
    if (journal == NULL || !journal->initialized || index >= SETTINGS_JOURNAL_VALUE_COUNT) {
        return false;
    }
    uint8_t logical_page = (uint8_t)(index / SETTINGS_PER_LOGICAL_PAGE);
    uint8_t local_index = (uint8_t)(index % SETTINGS_PER_LOGICAL_PAGE);
    uint16_t current;
    JournalSearchResult current_result =
        local_value_find(journal, logical_page, local_index, &current);
    if (current_result == JOURNAL_SEARCH_ERROR) {
        return false;
    }
    if (current_result == JOURNAL_SEARCH_FOUND && current == value) {
        return true;
    }

    uint16_t instruction;
    JournalSearchResult free_result = free_instruction_find(journal, logical_page, &instruction);
    if (free_result == JOURNAL_SEARCH_ERROR) {
        return false;
    }
    if (free_result == JOURNAL_SEARCH_ABSENT) {
        if (!logical_page_compact(journal, logical_page) ||
            free_instruction_find(journal, logical_page, &instruction) != JOURNAL_SEARCH_FOUND) {
            return false;
        }
    }

    uint8_t page = physical_page(logical_page, journal->active_slot[logical_page]);
    SettingsJournalInstruction record = {.value = value, .tag = local_index};
    if (!instruction_program(journal, page, instruction, record)) {
        return false;
    }
    return instruction != SETTINGS_JOURNAL_PAGE_INSTRUCTION_COUNT - 1 ||
           logical_page_compact(journal, logical_page);
}
