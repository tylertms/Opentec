#include "profile/bank.h"

#include <stdbool.h>
#include <stdint.h>

void tuning_profile_bank_defaults(TuningProfileBank *bank) {
    for (uint8_t slot = 0; slot < TUNING_PROFILE_SLOT_COUNT; slot++) {
        tuning_profile_defaults(&bank->slots[slot]);
    }
    bank->selected_slot = 0;
    bank->active_slot = 0;
    bank->standard_mode_enabled = true;
    bank->automatic_apply_pending = false;
}

bool tuning_profile_bank_select(TuningProfileBank *bank, uint8_t slot) {
    if (slot >= TUNING_PROFILE_SLOT_COUNT) {
        return false;
    }
    bank->selected_slot = slot;
    return true;
}

void tuning_profile_bank_activate_selected(TuningProfileBank *bank) {
    bank->active_slot = bank->selected_slot;
}

bool tuning_profile_bank_store(TuningProfileBank *bank, uint8_t slot,
                               const TuningProfile *profile) {
    if (slot >= TUNING_PROFILE_SLOT_COUNT) {
        return false;
    }
    bank->slots[slot] = *profile;
    tuning_profile_normalize(&bank->slots[slot]);
    return true;
}

bool tuning_profile_bank_set_standard_mode(TuningProfileBank *bank, bool enabled) {
    if (bank->standard_mode_enabled == enabled) {
        return false;
    }
    bank->standard_mode_enabled = enabled;
    if (enabled) {
        bank->selected_slot = 0;
        bank->active_slot = 0;
        tuning_profile_defaults(&bank->slots[1]);
    }
    return true;
}

const TuningProfile *tuning_profile_bank_selected(const TuningProfileBank *bank) {
    return &bank->slots[bank->selected_slot];
}

const TuningProfile *tuning_profile_bank_active(const TuningProfileBank *bank) {
    return &bank->slots[bank->active_slot];
}
