#include "profile/bank.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Restores all tuning setups and bank state to device defaults.
 *
 * Initializes six setups, selects and activates setup 1, and enables Standard mode.
 *
 * @param[out] bank Tuning-profile bank to initialize.
 */
void tuning_profile_bank_defaults(TuningProfileBank *bank) {
    for (uint8_t slot = 0; slot < TUNING_PROFILE_SLOT_COUNT; slot++) {
        tuning_profile_defaults(&bank->slots[slot]);
    }
    bank->selected_slot = 0;
    bank->active_slot = 0;
    bank->standard_mode_enabled = true;
}

/**
 * @brief Selects a tuning setup without activating it.
 *
 * Preserves the current selection when the zero-based setup index is outside the six-slot bank.
 *
 * @param[in,out] bank Tuning-profile bank to update.
 * @param[in] slot Zero-based setup index.
 * @return True when the setup was selected.
 */
bool tuning_profile_bank_select(TuningProfileBank *bank, uint8_t slot) {
    if (slot >= TUNING_PROFILE_SLOT_COUNT) {
        return false;
    }
    bank->selected_slot = slot;
    return true;
}

/**
 * @brief Activates the selected tuning setup.
 *
 * Makes the selected setup the source for runtime tuning behavior.
 *
 * @param[in,out] bank Tuning-profile bank to update.
 */
void tuning_profile_bank_activate_selected(TuningProfileBank *bank) {
    bank->active_slot = bank->selected_slot;
}

/**
 * @brief Stores a normalized tuning setup.
 *
 * Copies a logical profile into a valid zero-based setup slot and applies all supported limits.
 *
 * @param[in,out] bank Tuning-profile bank to update.
 * @param[in] slot Zero-based destination setup index.
 * @param[in] profile Logical profile to store.
 * @return True when the setup was stored.
 */
bool tuning_profile_bank_store(TuningProfileBank *bank, uint8_t slot,
                               const TuningProfile *profile) {
    if (slot >= TUNING_PROFILE_SLOT_COUNT) {
        return false;
    }
    bank->slots[slot] = *profile;
    tuning_profile_normalize(&bank->slots[slot]);
    return true;
}

/**
 * @brief Returns the selected tuning setup.
 *
 * Provides the profile currently chosen for inspection or activation.
 *
 * @param[in] bank Tuning-profile bank to inspect.
 * @return The selected tuning setup.
 */
const TuningProfile *tuning_profile_bank_selected(const TuningProfileBank *bank) {
    return &bank->slots[bank->selected_slot];
}

/**
 * @brief Returns the active tuning setup.
 *
 * Provides the profile that supplies current runtime tuning behavior.
 *
 * @param[in] bank Tuning-profile bank to inspect.
 * @return The active tuning setup.
 */
const TuningProfile *tuning_profile_bank_active(const TuningProfileBank *bank) {
    return &bank->slots[bank->active_slot];
}
