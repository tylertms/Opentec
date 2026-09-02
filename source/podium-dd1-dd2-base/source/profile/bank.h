#ifndef OPENTEC_BASE_PROFILE_BANK_H
#define OPENTEC_BASE_PROFILE_BANK_H

#include <stdbool.h>
#include <stdint.h>

#include "profile/tuning.h"

/** @brief Number of tuning setups retained in one profile bank. */
enum { TUNING_PROFILE_SLOT_COUNT = 6 /**< Number of stored tuning setups. */ };

/** @brief Tuning setups, selection state, mode, and transient Auto status. */
typedef struct {
    TuningProfile slots[TUNING_PROFILE_SLOT_COUNT]; /**< Stored tuning setups. */
    uint8_t selected_slot;                          /**< Zero-based setup selected for editing. */
    uint8_t active_slot;                            /**< Zero-based setup active at runtime. */
    bool standard_mode_enabled;                     /**< True when Standard mode is selected. */
    bool automatic_apply_pending;                   /**< True when Auto values await application. */
} TuningProfileBank;

/**
 * @brief Restores a profile bank to its device defaults.
 *
 * Initializes every stored setup, selects and activates setup zero, enables Standard mode, and
 * clears the transient Auto apply-pending marker.
 *
 * @param[out] bank Profile bank to initialize.
 */
void tuning_profile_bank_defaults(TuningProfileBank *bank);

/**
 * @brief Selects a setup without activating it.
 *
 * Leaves the existing selection unchanged when slot is outside the retained bank.
 *
 * @param[in,out] bank Profile bank to update.
 * @param[in] slot Zero-based setup index.
 * @return true when slot was selected; false when slot is outside the bank.
 */
bool tuning_profile_bank_select(TuningProfileBank *bank, uint8_t slot);

/**
 * @brief Activates the selected setup.
 *
 * Copies the selected slot index into the active slot index for runtime use.
 *
 * @param[in,out] bank Profile bank to update.
 */
void tuning_profile_bank_activate_selected(TuningProfileBank *bank);

/**
 * @brief Stores and normalizes one setup.
 *
 * Copies profile into the requested slot and applies all profile limits and enum normalization.
 *
 * @param[in,out] bank Profile bank to update.
 * @param[in] slot Zero-based destination setup index.
 * @param[in] profile Profile to copy and normalize.
 * @return true when profile was stored; false when slot is outside the bank.
 */
bool tuning_profile_bank_store(TuningProfileBank *bank, uint8_t slot, const TuningProfile *profile);

/**
 * @brief Selects Standard or Advanced mode.
 *
 * Entering Standard mode selects and activates slot zero and restores slot one to defaults;
 * entering Advanced mode preserves the current selection and activation.
 *
 * @param[in,out] bank Profile bank to update.
 * @param[in] enabled true to select Standard mode, or false to select Advanced mode.
 * @return true when the mode changed; false when it was already enabled or disabled as requested.
 */
bool tuning_profile_bank_set_standard_mode(TuningProfileBank *bank, bool enabled);

/**
 * @brief Returns the selected setup.
 *
 * Provides the setup currently selected for inspection or activation.
 *
 * @param[in] bank Profile bank to inspect.
 * @return Pointer to the selected setup.
 */
const TuningProfile *tuning_profile_bank_selected(const TuningProfileBank *bank);

/**
 * @brief Returns the active setup.
 *
 * Provides the setup currently supplying runtime tuning behavior.
 *
 * @param[in] bank Profile bank to inspect.
 * @return Pointer to the active setup.
 */
const TuningProfile *tuning_profile_bank_active(const TuningProfileBank *bank);

#endif
